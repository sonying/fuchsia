// Copyright 2020 The Fuchsia Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package main

import (
	"flag"
	"log"
	"os"

	"go.fuchsia.dev/fuchsia/tools/fidl/fidlgen_hlcpp/codegen"
	fidl "go.fuchsia.dev/fuchsia/tools/fidl/lib/fidlgen"
)

// TODO(fxbug.dev/45483): Until all SDK consumers are moved off to using dedicated
// flags to invoke HLCPP fidlgen, we must preserve this legacy flags.
var jsonPath = flag.String("json", "",
	"relative path to the FIDL intermediate representation.")
var outputBase = flag.String("output-base", "",
	"the base file name for files generated by this generator.")
var includeBase = flag.String("include-base", "",
	"the directory to which C and C++ includes should be relative.")
var includeStem = flag.String("include-stem", "cpp/fidl",
	"[optional] the suffix after library path when referencing includes. "+
		"Includes will be of the form <my/library/{include-stem}.h>. ")
var generatorsUnused = flag.String("generators", "",
	"unused")
var clangFormatPath = flag.String("clang-format-path", "",
	"path to the clang-format tool.")

// These options support only generating the domain objects and using them in LLCPP.
var splitGenerationDomainObjects = flag.Bool("experimental-split-generation-domain-objects", false,
	"[optional] only generate the domain object definitions for the data types in this library.")

func flagsValid() bool {
	return *jsonPath != "" && *outputBase != "" && *includeBase != ""
}

func main() {
	flag.Parse()
	if !flag.Parsed() || !flagsValid() {
		flag.PrintDefaults()
		os.Exit(1)
	}

	ir, err := fidl.ReadJSONIr(*jsonPath)
	if err != nil {
		log.Fatal(err)
	}

	mode := codegen.Monolithic
	if *splitGenerationDomainObjects {
		mode = codegen.OnlyGenerateDomainObjects
	}
	config := codegen.Config{
		OutputBase:  *outputBase,
		IncludeBase: *includeBase,
		IncludeStem: *includeStem,
	}
	generator := codegen.NewFidlGenerator(mode)
	if err := generator.GenerateFidl(ir, &config, *clangFormatPath); err != nil {
		log.Fatalf("Error running generator: %v", err)
	}
}
