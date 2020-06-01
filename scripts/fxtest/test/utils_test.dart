import 'package:test/test.dart';
import 'package:fxtest/fxtest.dart';

void main() {
  group('fuchsia-package URLs are correctly parsed', () {
    test('when all components are present', () {
      PackageUrl packageUrl = PackageUrl.fromString(
          'fuchsia-pkg://myroot.com/pkg-name/VARIANT?hash=asdf#OMG.cmx');
      expect(packageUrl.host, 'myroot.com');
      expect(packageUrl.packageName, 'pkg-name');
      expect(packageUrl.packageVariant, 'VARIANT');
      expect(packageUrl.hash, 'asdf');
      expect(packageUrl.fullComponentName, 'OMG.cmx');
      expect(packageUrl.componentName, 'OMG');
    });

    test('when all components are present plus meta', () {
      PackageUrl packageUrl = PackageUrl.fromString(
          'fuchsia-pkg://myroot.com/pkg-name/VARIANT?hash=asdf#meta/OMG.cmx');
      expect(packageUrl.host, 'myroot.com');
      expect(packageUrl.packageName, 'pkg-name');
      expect(packageUrl.packageVariant, 'VARIANT');
      expect(packageUrl.hash, 'asdf');
      expect(packageUrl.fullComponentName, 'OMG.cmx');
      expect(packageUrl.componentName, 'OMG');
    });

    test('when the variant is missing', () {
      PackageUrl packageUrl = PackageUrl.fromString(
          'fuchsia-pkg://myroot.com/pkg-name?hash=asdf#OMG.cmx');
      expect(packageUrl.host, 'myroot.com');
      expect(packageUrl.packageName, 'pkg-name');
      expect(packageUrl.packageVariant, null);
      expect(packageUrl.hash, 'asdf');
      expect(packageUrl.fullComponentName, 'OMG.cmx');
      expect(packageUrl.componentName, 'OMG');
    });

    test('when the hash is missing', () {
      PackageUrl packageUrl = PackageUrl.fromString(
          'fuchsia-pkg://myroot.com/pkg-name/VARIANT#OMG.cmx');
      expect(packageUrl.host, 'myroot.com');
      expect(packageUrl.packageName, 'pkg-name');
      expect(packageUrl.packageVariant, 'VARIANT');
      expect(packageUrl.hash, null);
      expect(packageUrl.fullComponentName, 'OMG.cmx');
      expect(packageUrl.componentName, 'OMG');
    });

    test('when the resource path is missing', () {
      PackageUrl packageUrl = PackageUrl.fromString(
          'fuchsia-pkg://myroot.com/pkg-name/VARIANT?hash=asdf');
      expect(packageUrl.host, 'myroot.com');
      expect(packageUrl.packageName, 'pkg-name');
      expect(packageUrl.packageVariant, 'VARIANT');
      expect(packageUrl.hash, 'asdf');
      expect(packageUrl.fullComponentName, '');
      expect(packageUrl.componentName, '');
    });

    test('when the variant and hash are missing', () {
      PackageUrl packageUrl =
          PackageUrl.fromString('fuchsia-pkg://myroot.com/pkg-name#OMG.cmx');
      expect(packageUrl.host, 'myroot.com');
      expect(packageUrl.packageName, 'pkg-name');
      expect(packageUrl.packageVariant, null);
      expect(packageUrl.hash, null);
      expect(packageUrl.fullComponentName, 'OMG.cmx');
      expect(packageUrl.componentName, 'OMG');
    });
    test('when the variant and resource path are missing', () {
      PackageUrl packageUrl =
          PackageUrl.fromString('fuchsia-pkg://myroot.com/pkg-name?hash=asdf');
      expect(packageUrl.host, 'myroot.com');
      expect(packageUrl.packageName, 'pkg-name');
      expect(packageUrl.packageVariant, null);
      expect(packageUrl.hash, 'asdf');
      expect(packageUrl.fullComponentName, '');
      expect(packageUrl.componentName, '');
    });

    test('when the hash and resource path are missing', () {
      PackageUrl packageUrl =
          PackageUrl.fromString('fuchsia-pkg://myroot.com/pkg-name/VARIANT');
      expect(packageUrl.host, 'myroot.com');
      expect(packageUrl.packageName, 'pkg-name');
      expect(packageUrl.packageVariant, 'VARIANT');
      expect(packageUrl.hash, null);
      expect(packageUrl.fullComponentName, '');
      expect(packageUrl.componentName, '');
    });

    test('when the variant, hash, and resource path are missing', () {
      PackageUrl packageUrl =
          PackageUrl.fromString('fuchsia-pkg://myroot.com/pkg-name');
      expect(packageUrl.host, 'myroot.com');
      expect(packageUrl.packageName, 'pkg-name');
      expect(packageUrl.packageVariant, null);
      expect(packageUrl.hash, null);
      expect(packageUrl.fullComponentName, '');
      expect(packageUrl.componentName, '');
    });
  });

  group('slash splitter', () {
    var sep = '/';
    test('correctly handles null', () {
      expect(chunkOnSlashes(null, null, sep: sep), <String>[]);
      expect(chunkOnSlashes('asdf', null, sep: sep), <String>['asdf']);
      expect(chunkOnSlashes(null, 'asdf', sep: sep), <String>[]);
    });

    test('correctly handles empty strings', () {
      expect(chunkOnSlashes('', '', sep: sep), <String>['']);
      expect(chunkOnSlashes('', 'asdf', sep: sep), <String>['']);
      expect(chunkOnSlashes('asdf', '', sep: sep), <String>['asdf']);

      expect(chunkOnSlashes('', 'asdf/asdf', sep: sep), <String>['']);
      expect(chunkOnSlashes('asdf/asdf', '', sep: sep), <String>['asdf']);
      expect(
          chunkOnSlashes('asdf/ASDF', '', sep: sep), <String>['asdf', 'ASDF']);
    });

    test('correctly handles one slash in needle', () {
      expect(chunkOnSlashes('', 'asdf/asdf', sep: sep), <String>['']);
      expect(chunkOnSlashes('TEST', 'asdf/asdf', sep: sep), <String>['TEST']);
      expect(chunkOnSlashes('abc/abc', 'asdf/asdf', sep: sep),
          <String>['abc/abc']);
      expect(chunkOnSlashes('abc/ABC/xyz', 'asdf/asdf', sep: sep), <String>[
        'abc/ABC',
        'ABC/xyz',
      ]);
      expect(chunkOnSlashes('abc/ABC/xyz/abc', 'asdf/asdf', sep: sep), <String>[
        'abc/ABC',
        'ABC/xyz',
        'xyz/abc',
      ]);
    });

    test('correctly handles more than one slash in needle', () {
      expect(chunkOnSlashes('', 'asdf/asdf/asdf', sep: sep), <String>['']);
      expect(
          chunkOnSlashes('TEST', 'asdf/asdf/asdf', sep: sep), <String>['TEST']);
      expect(chunkOnSlashes('abc/abc', 'asdf/asdf/asdf', sep: sep),
          <String>['abc/abc']);
      expect(
          chunkOnSlashes('abc/ABC/xyz', 'asdf/asdf/asdf', sep: sep), <String>[
        'abc/ABC/xyz',
      ]);
      expect(
          chunkOnSlashes('abc/ABC/xyz/abc', 'asdf/asdf/asdf', sep: sep),
          <String>[
            'abc/ABC/xyz',
            'ABC/xyz/abc',
          ]);
    });

    test('correctly handles leading slashes', () {
      expect(
        chunkOnSlashes('//some/test', 'a/a'),
        <String>['/', '/some', 'some/test'],
      );
    });
  });

  group('fuzzy comparer correctly evaluates', () {
    var fuzzy = FuzzyComparer(threshold: 4);
    test('equals with no slashes', () {
      expect(fuzzy.equals('asdf', 'asdF').isMatch, true);
      expect(fuzzy.equals('asdf', 'asDF').isMatch, true);
      expect(fuzzy.equals('asdf', 'aSDF').isMatch, true);
      expect(fuzzy.equals('asdf', 'ASDF').isMatch, false);
    });
    test('equals with one slash in haystack', () {
      expect(fuzzy.equals('ASDF/asdf', 'asdf').isMatch, false);
      expect(fuzzy.equals('/asdf', 'asdf').isMatch, true);
      expect(fuzzy.equals('/asdf', 'asdF').isMatch, true);
    });
    test('equals with one slash in needle', () {
      expect(fuzzy.equals('asdf', 'asdf/ASDF').isMatch, false);
      expect(fuzzy.equals('asdf/asdF', 'asdf/ASDF').isMatch, true);
      expect(fuzzy.equals('asdf/asdf', 'asdf/ASDF').isMatch, false);
    });
    test('contains with zero slashes in the needle', () {
      expect(fuzzy.contains('xyz/ASDF/XYZ', 'asdf').isMatch, false);
      expect(fuzzy.contains('xyz/ASDF/abc', 'asdf').isMatch,
          true); // abc -> asdf == 3
      expect(fuzzy.contains('xyz/aSDF/abc', 'asdf').isMatch, true);
      expect(fuzzy.contains('xyz/asDF/abc', 'asdf').isMatch, true);
      expect(fuzzy.contains('xyz/asdF/abc', 'asdf').isMatch, true);
      expect(fuzzy.contains('xyz/asdf/abc', 'asdf').isMatch, true);
    });
    test('contains with one slash in the needle', () {
      expect(fuzzy.contains('abc/asdf/ASDF', 'asdf/ASDF').isMatch, true);
      expect(fuzzy.contains('abc/asdf/aSDF', 'asdf/ASDF').isMatch, true);
      expect(fuzzy.contains('abc/asdf/asDF', 'asdf/ASDF').isMatch, true);
      expect(fuzzy.contains('abc/asdf/asdF', 'asdf/ASDF').isMatch, true);
      expect(fuzzy.contains('abc/asdf/asdf', 'asdf/ASDF').isMatch, false);
    });
    test('contains with two slashes in the needle', () {
      expect(
          fuzzy.contains('abc/asdf/ASDF/xyz', 'asdf/ASDF/xyz').isMatch, true);
      expect(
          fuzzy.contains('abc/asdf/aSDF/xyZ', 'asdf/ASDF/xyz').isMatch, true);
      expect(fuzzy.contains('abc/asdf/asdf', 'asdf/ASDF/xyz').isMatch, false);
      expect(fuzzy.contains('abc/asdf', 'asdf/ASDF/xyz').isMatch, false);
      expect(fuzzy.contains('abc', 'asdf/ASDF/xyz').isMatch, false);
      expect(fuzzy.contains('abc/abc/abc', 'asdf/ASDF/xyz').isMatch, false);
      expect(fuzzy.contains('asdf/ASDF/xyz', 'asdf/ASDF/xyz').isMatch, true);
      expect(fuzzy.contains('asdf/ASDF/XYZ', 'asdf/ASDF/xyz').isMatch, true);
    });
    test('endsWith with zero slashes in the needle', () {
      expect(fuzzy.endsWith('asdf/ABC', 'asdf').isMatch, false);
      expect(fuzzy.endsWith('xyz/asdf', 'asdf').isMatch, true);
      expect(fuzzy.endsWith('xyz/aSDF', 'asdf').isMatch, true);
      expect(fuzzy.endsWith('xyz/asdf', 'asdf').isMatch, true);
    });
    test('endsWith with more than 1 slash in the needle', () {
      expect(fuzzy.endsWith('asdf/ABC', 'asdf/ABC').isMatch, true);
      expect(fuzzy.endsWith('asdf/aaabc', 'asdf/ABC').isMatch, false);
      expect(fuzzy.endsWith('asdf/abc', 'asdf/ABC').isMatch, true);
      expect(fuzzy.endsWith('asdf/aBC', 'asdf/ABC').isMatch, true);
      expect(fuzzy.endsWith('xyz/asdf/ABC', 'asdf/ABC').isMatch, true);
      expect(fuzzy.endsWith('xyz/asdf/aaabc', 'asdf/ABC').isMatch, false);
      expect(fuzzy.endsWith('xyz/asdf/abc', 'asdf/ABC').isMatch, true);
      expect(fuzzy.endsWith('xyz/asdf/aBC', 'asdf/ABC').isMatch, true);
      expect(fuzzy.endsWith('asdf/ABC/xyz', 'asdf/ABC').isMatch, false);
      expect(fuzzy.endsWith('asdf/aaabc/xyz', 'asdf/ABC').isMatch, false);
      expect(fuzzy.endsWith('asdf/abc/xyz', 'asdf/ABC').isMatch, false);
      expect(fuzzy.endsWith('asdf/aBC/xyz', 'asdf/ABC').isMatch, false);
    });
    test('startsWith with zero slashes in the needle', () {
      expect(fuzzy.startsWith('asdf/ABC', 'asdf').isMatch, true);
      expect(fuzzy.startsWith('xyz/asdf', 'asdf').isMatch, false);
      expect(fuzzy.startsWith('aSDF/xyz', 'asdf').isMatch, true);
      expect(fuzzy.startsWith('asdf/xyz', 'asdf').isMatch, true);
    });
    test('startsWith with more than 1 slash in the needle', () {
      expect(fuzzy.startsWith('asdf/ABC', 'asdf/ABC').isMatch, true);
      expect(fuzzy.startsWith('asdf/aaabc', 'asdf/ABC').isMatch, false);
      expect(fuzzy.startsWith('asdf/abc', 'asdf/ABC').isMatch, true);
      expect(fuzzy.startsWith('asdf/aBC', 'asdf/ABC').isMatch, true);
      expect(fuzzy.startsWith('xyz/asdf/ABC', 'asdf/ABC').isMatch, false);
      expect(fuzzy.startsWith('xyz/asdf/aaabc', 'asdf/ABC').isMatch, false);
      expect(fuzzy.startsWith('xyz/asdf/abc', 'asdf/ABC').isMatch, false);
      expect(fuzzy.startsWith('xyz/asdf/aBC', 'asdf/ABC').isMatch, false);
      expect(fuzzy.startsWith('asdf/ABC/xyz', 'asdf/ABC').isMatch, true);
      expect(fuzzy.startsWith('asdf/aaabc/xyz', 'asdf/ABC').isMatch, false);
      expect(fuzzy.startsWith('asdf/abc/xyz', 'asdf/ABC').isMatch, true);
      expect(fuzzy.startsWith('asdf/aBC/xyz', 'asdf/ABC').isMatch, true);
    });

    test('when needle has bookend slashes', () {
      // The Levenshtein distance of abc -> ABC is 3, which is the maximum
      // allowed given our threshold of 4. This tests verifies that the
      // trailing slash in our needle does not ruin results.
      expect(fuzzy.endsWith('abc/asdf/ABC', 'asdf/abc/').isMatch, true);
      expect(fuzzy.startsWith('asdf/ABC', '/asdf/abc/').isMatch, true);
    });
  });

  group('confidence computed correctly', () {
    test('with threshold of 5', () {
      var comparer = FuzzyComparer(threshold: 5);
      expect(comparer.computeConfidence(1), 0.8);
      expect(comparer.computeConfidence(2), 0.6);
      expect(comparer.computeConfidence(3), 0.4);
      expect(comparer.computeConfidence(4), 0.2);
      expect(comparer.computeConfidence(5), 0);
    });
  });
}
