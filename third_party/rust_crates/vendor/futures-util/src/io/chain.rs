use futures_core::task::{Context, Poll};
#[cfg(feature = "read-initializer")]
use futures_io::Initializer;
use futures_io::{AsyncBufRead, AsyncRead, IoSliceMut};
use pin_project::{pin_project, project};
use std::fmt;
use std::io;
use std::pin::Pin;

/// Reader for the [`chain`](super::AsyncReadExt::chain) method.
#[pin_project]
#[must_use = "readers do nothing unless polled"]
pub struct Chain<T, U> {
    #[pin]
    first: T,
    #[pin]
    second: U,
    done_first: bool,
}

impl<T, U> Chain<T, U>
where
    T: AsyncRead,
    U: AsyncRead,
{
    pub(super) fn new(first: T, second: U) -> Self {
        Self {
            first,
            second,
            done_first: false,
        }
    }

    /// Gets references to the underlying readers in this `Chain`.
    pub fn get_ref(&self) -> (&T, &U) {
        (&self.first, &self.second)
    }

    /// Gets mutable references to the underlying readers in this `Chain`.
    ///
    /// Care should be taken to avoid modifying the internal I/O state of the
    /// underlying readers as doing so may corrupt the internal state of this
    /// `Chain`.
    pub fn get_mut(&mut self) -> (&mut T, &mut U) {
        (&mut self.first, &mut self.second)
    }

    /// Gets pinned mutable references to the underlying readers in this `Chain`.
    ///
    /// Care should be taken to avoid modifying the internal I/O state of the
    /// underlying readers as doing so may corrupt the internal state of this
    /// `Chain`.
    pub fn get_pin_mut(self: Pin<&mut Self>) -> (Pin<&mut T>, Pin<&mut U>) {
        unsafe {
            let Self { first, second, .. } = self.get_unchecked_mut();
            (Pin::new_unchecked(first), Pin::new_unchecked(second))
        }
    }

    /// Consumes the `Chain`, returning the wrapped readers.
    pub fn into_inner(self) -> (T, U) {
        (self.first, self.second)
    }
}

impl<T, U> fmt::Debug for Chain<T, U>
where
    T: fmt::Debug,
    U: fmt::Debug,
{
    fn fmt(&self, f: &mut fmt::Formatter<'_>) -> fmt::Result {
        f.debug_struct("Chain")
            .field("t", &self.first)
            .field("u", &self.second)
            .field("done_first", &self.done_first)
            .finish()
    }
}

impl<T, U> AsyncRead for Chain<T, U>
where
    T: AsyncRead,
    U: AsyncRead,
{
    #[project]
    fn poll_read(
        self: Pin<&mut Self>,
        cx: &mut Context<'_>,
        buf: &mut [u8],
    ) -> Poll<io::Result<usize>> {
        #[project]
        let Chain { first, second, done_first } = self.project();

        if !*done_first {
            match ready!(first.poll_read(cx, buf)?) {
                0 if !buf.is_empty() => *done_first = true,
                n => return Poll::Ready(Ok(n)),
            }
        }
        second.poll_read(cx, buf)
    }

    #[project]
    fn poll_read_vectored(
        self: Pin<&mut Self>,
        cx: &mut Context<'_>,
        bufs: &mut [IoSliceMut<'_>],
    ) -> Poll<io::Result<usize>> {
        #[project]
        let Chain { first, second, done_first } = self.project();

        if !*done_first {
            let n = ready!(first.poll_read_vectored(cx, bufs)?);
            if n == 0 && bufs.iter().any(|b| !b.is_empty()) {
                *done_first = true
            } else {
                return Poll::Ready(Ok(n));
            }
        }
        second.poll_read_vectored(cx, bufs)
    }

    #[cfg(feature = "read-initializer")]
    unsafe fn initializer(&self) -> Initializer {
        let initializer = self.first.initializer();
        if initializer.should_initialize() {
            initializer
        } else {
            self.second.initializer()
        }
    }
}

impl<T, U> AsyncBufRead for Chain<T, U>
where
    T: AsyncBufRead,
    U: AsyncBufRead,
{
    #[project]
    fn poll_fill_buf(self: Pin<&mut Self>, cx: &mut Context<'_>) -> Poll<io::Result<&[u8]>> {
        #[project]
        let Chain { first, second, done_first } = self.project();

        if !*done_first {
            match ready!(first.poll_fill_buf(cx)?) {
                buf if buf.is_empty() => {
                    *done_first = true;
                }
                buf => return Poll::Ready(Ok(buf)),
            }
        }
        second.poll_fill_buf(cx)
    }

    #[project]
    fn consume(self: Pin<&mut Self>, amt: usize) {
        #[project]
        let Chain { first, second, done_first } = self.project();

        if !*done_first {
            first.consume(amt)
        } else {
            second.consume(amt)
        }
    }
}
