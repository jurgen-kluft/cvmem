package cvmem

import (
	cbase "github.com/jurgen-kluft/cbase/package"
	denv "github.com/jurgen-kluft/ccode/denv"
	ccore "github.com/jurgen-kluft/ccore/package"
	cunittest "github.com/jurgen-kluft/cunittest/package"
)

// GetPackage returns the package object of 'cvmem'
func GetPackage() *denv.Package {
	// Dependencies
	cunittestpkg := cunittest.GetPackage()
	ccorepkg := ccore.GetPackage()
	cbasepkg := cbase.GetPackage()

	// The main (cvmem) package
	mainpkg := denv.NewPackage("cvmem")
	mainpkg.AddPackage(cunittestpkg)
	mainpkg.AddPackage(ccorepkg)
	mainpkg.AddPackage(cbasepkg)

	// 'cvmem' library
	mainlib := denv.SetupDefaultCppLibProject("cvmem", "github.com\\jurgen-kluft\\cvmem")
	mainlib.Dependencies = append(mainlib.Dependencies, ccorepkg.GetMainLib())
	mainlib.Dependencies = append(mainlib.Dependencies, cbasepkg.GetMainLib())

	// 'cvmem' unittest project
	maintest := denv.SetupDefaultCppTestProject("cvmem_test", "github.com\\jurgen-kluft\\cvmem")
	maintest.Dependencies = append(maintest.Dependencies, cunittestpkg.GetMainLib())
	maintest.Dependencies = append(maintest.Dependencies, cbasepkg.GetMainLib())
	maintest.Dependencies = append(maintest.Dependencies, ccorepkg.GetMainLib())
	maintest.Dependencies = append(maintest.Dependencies, mainlib)

	mainpkg.AddMainLib(mainlib)
	mainpkg.AddUnittest(maintest)
	return mainpkg
}
