package cvmem

import (
	denv "github.com/jurgen-kluft/ccode/denv"
	ccore "github.com/jurgen-kluft/ccore/package"
	centry "github.com/jurgen-kluft/centry/package"
	cunittest "github.com/jurgen-kluft/cunittest/package"
)

// GetPackage returns the package object of 'cvmem'
func GetPackage() *denv.Package {
	// Dependencies
	cunittestpkg := cunittest.GetPackage()
	centrypkg := centry.GetPackage()
	ccorepkg := ccore.GetPackage()

	// The main (cvmem) package
	mainpkg := denv.NewPackage("cvmem")
	mainpkg.AddPackage(cunittestpkg)
	mainpkg.AddPackage(centrypkg)
	mainpkg.AddPackage(ccorepkg)

	// 'cvmem' library
	mainlib := denv.SetupDefaultCppLibProject("cvmem", "github.com\\jurgen-kluft\\cvmem")
	mainlib.Dependencies = append(mainlib.Dependencies, ccorepkg.GetMainLib())

	// 'cvmem' unittest project
	maintest := denv.SetupDefaultCppTestProject("cvmem_test", "github.com\\jurgen-kluft\\cvmem")
	maintest.Dependencies = append(maintest.Dependencies, cunittestpkg.GetMainLib())
	maintest.Dependencies = append(maintest.Dependencies, centrypkg.GetMainLib())
	maintest.Dependencies = append(maintest.Dependencies, ccorepkg.GetMainLib())
	maintest.Dependencies = append(maintest.Dependencies, mainlib)

	mainpkg.AddMainLib(mainlib)
	mainpkg.AddUnittest(maintest)
	return mainpkg
}
