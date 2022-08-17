package main

import (
	ccode "github.com/jurgen-kluft/ccode"
	cpkg "github.com/jurgen-kluft/cvmem/package"
)

func main() {
	ccode.Generate(cpkg.GetPackage())
}
