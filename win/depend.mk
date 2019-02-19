#
# Dependencies:
#
# This file was automatically generated using helper "depend.tcl" for target "win".
#

## header includes:

### -- generic --
regcustom.h : regex.h
regex.h : tclInt.h
regguts.h : regcustom.h
tcl.h : tclDecls.h tclPlatDecls.h
tclCompile.h : tclInt.h
tclFileSystem.h : tcl.h
tclInt.h : tclIntDecls.h tclIntPlatDecls.h tclPort.h tclTomMathDecls.h
tclOO.h : tcl.h tclOODecls.h
tclOOInt.h : tclInt.h tclOO.h tclOOIntDecls.h
tclPort.h : tcl.h tclWinPort.h
tclRegexp.h : regex.h
tclTomMath.h : tclTomMathDecls.h
tclTomMathDecls.h : tcl.h
tclTomMathInt.h : tclInt.h tclTomMath.h
tommath.h : tclTomMathInt.h
### -- win --
tclWinInt.h : tclInt.h

## includes:

### -- generic --
regcomp.$(OBJEXT) : regguts.h
regerror.$(OBJEXT) : regerrs.h regguts.h
regexec.$(OBJEXT) : regguts.h
regfree.$(OBJEXT) : regguts.h
regfronts.$(OBJEXT) : regguts.h
tclAlloc.$(OBJEXT) : tclInt.h
tclAssembly.$(OBJEXT) : tclCompile.h tclInt.h tclOOInt.h
tclAsync.$(OBJEXT) : tclInt.h
tclBasic.$(OBJEXT) : tclCompile.h tclInt.h tclOOInt.h tommath.h
tclBinary.$(OBJEXT) : tclInt.h tommath.h
tclCkalloc.$(OBJEXT) : tclInt.h
tclClock.$(OBJEXT) : tclInt.h
tclCmdAH.$(OBJEXT) : tclInt.h tclWinInt.h
tclCmdIL.$(OBJEXT) : tclInt.h tclRegexp.h
tclCmdMZ.$(OBJEXT) : tclInt.h tclRegexp.h tclStringTrim.h
tclCompCmds.$(OBJEXT) : tclCompile.h tclInt.h
tclCompCmdsGR.$(OBJEXT) : tclCompile.h tclInt.h
tclCompCmdsSZ.$(OBJEXT) : tclCompile.h tclInt.h tclStringTrim.h
tclCompExpr.$(OBJEXT) : tclCompile.h tclInt.h
tclCompile.$(OBJEXT) : tclCompile.h tclInt.h
tclConfig.$(OBJEXT) : tclInt.h
tclDate.$(OBJEXT) : tclInt.h
tclDictObj.$(OBJEXT) : tclInt.h tommath.h
tclDisassemble.$(OBJEXT) : tclCompile.h tclInt.h tclOOInt.h
tclEncoding.$(OBJEXT) : tclInt.h
tclEnsemble.$(OBJEXT) : tclCompile.h tclInt.h
tclEnv.$(OBJEXT) : tclInt.h
tclEvent.$(OBJEXT) : tclInt.h
tclExecute.$(OBJEXT) : tclCompile.h tclInt.h tclOOInt.h tommath.h
tclFCmd.$(OBJEXT) : tclFileSystem.h tclInt.h
tclFileName.$(OBJEXT) : tclFileSystem.h tclInt.h tclRegexp.h
tclGet.$(OBJEXT) : tclInt.h
tclHash.$(OBJEXT) : tclInt.h
tclHistory.$(OBJEXT) : tclInt.h
tclIndexObj.$(OBJEXT) : tclInt.h
tclInterp.$(OBJEXT) : tclInt.h
tclIO.$(OBJEXT) : tclInt.h tclIO.h
tclIOCmd.$(OBJEXT) : tclInt.h
tclIOGT.$(OBJEXT) : tclInt.h tclIO.h
tclIORChan.$(OBJEXT) : tclInt.h tclIO.h
tclIORTrans.$(OBJEXT) : tclInt.h tclIO.h
tclIOSock.$(OBJEXT) : tclInt.h
tclIOUtil.$(OBJEXT) : tclFileSystem.h tclInt.h tclWinInt.h
tclLink.$(OBJEXT) : tclInt.h
tclListObj.$(OBJEXT) : tclInt.h
tclLiteral.$(OBJEXT) : tclCompile.h tclInt.h
tclLoad.$(OBJEXT) : tclInt.h
tclLoadNone.$(OBJEXT) : tclInt.h
tclMain.$(OBJEXT) : tclInt.h
tclNamesp.$(OBJEXT) : tclCompile.h tclInt.h
tclNotify.$(OBJEXT) : tclInt.h
tclObj.$(OBJEXT) : tclInt.h tommath.h
tclOO.$(OBJEXT) : tclInt.h tclOOInt.h
tclOOBasic.$(OBJEXT) : tclInt.h tclOOInt.h
tclOOCall.$(OBJEXT) : tclInt.h tclOOInt.h
tclOODefineCmds.$(OBJEXT) : tclInt.h tclOOInt.h
tclOOInfo.$(OBJEXT) : tclInt.h tclOOInt.h
tclOOMethod.$(OBJEXT) : tclCompile.h tclInt.h tclOOInt.h
tclOOStubInit.$(OBJEXT) : tclOOInt.h
tclOOStubLib.$(OBJEXT) : tclOOInt.h
tclOptimize.$(OBJEXT) : tclCompile.h tclInt.h
tclPanic.$(OBJEXT) : tclInt.h
tclParse.$(OBJEXT) : tclInt.h tclParse.h
tclPathObj - Copy.$(OBJEXT) : tclFileSystem.h tclInt.h
tclPathObj.$(OBJEXT) : tclFileSystem.h tclInt.h
tclPipe.$(OBJEXT) : tclInt.h
tclPkg.$(OBJEXT) : tclInt.h
tclPkgConfig.$(OBJEXT) : tclInt.h
tclPosixStr.$(OBJEXT) : tclInt.h
tclPreserve.$(OBJEXT) : tclInt.h
tclProc.$(OBJEXT) : tclCompile.h tclInt.h
tclRegexp.$(OBJEXT) : tclInt.h tclRegexp.h
tclResolve.$(OBJEXT) : tclInt.h
tclResult.$(OBJEXT) : tclInt.h
tclScan.$(OBJEXT) : tclInt.h
tclStringObj.$(OBJEXT) : tclInt.h tclStringRep.h tommath.h
tclStrToD.$(OBJEXT) : tclInt.h tommath.h
tclStubInit.$(OBJEXT) : tclInt.h tommath.h
tclStubLib.$(OBJEXT) : tclInt.h
tclStubLibTbl.$(OBJEXT) : tclInt.h
tclTest.$(OBJEXT) : tclInt.h tclIO.h tclOO.h tclRegexp.h
tclTestObj.$(OBJEXT) : tclInt.h tclStringRep.h tommath.h
tclTestProcBodyObj.$(OBJEXT) : tclInt.h
tclThread.$(OBJEXT) : tclInt.h
tclThreadAlloc.$(OBJEXT) : tclInt.h
tclThreadJoin.$(OBJEXT) : tclInt.h
tclThreadStorage.$(OBJEXT) : tclInt.h
tclThreadTest.$(OBJEXT) : tclInt.h
tclTimer.$(OBJEXT) : tclInt.h
tclTomMathInterface.$(OBJEXT) : tclInt.h tommath.h
tclTomMathStubLib.$(OBJEXT) : tclInt.h
tclTrace.$(OBJEXT) : tclInt.h
tclUtf.$(OBJEXT) : tclInt.h
tclUtil.$(OBJEXT) : tclInt.h tclParse.h tclStringTrim.h
tclVar.$(OBJEXT) : tclInt.h tclOOInt.h
tclZlib.$(OBJEXT) : tclInt.h tclIO.h
### -- win --
tclAppInit.$(OBJEXT) : tcl.h
tclWin32Dll.$(OBJEXT) : tclWinInt.h
tclWinChan.$(OBJEXT) : tclIO.h tclWinInt.h
tclWinConsole.$(OBJEXT) : tclWinInt.h
tclWinDde.$(OBJEXT) : tclInt.h
tclWinError.$(OBJEXT) : tclInt.h
tclWinFCmd.$(OBJEXT) : tclWinInt.h
tclWinFile.$(OBJEXT) : tclFileSystem.h tclWinInt.h
tclWinInit.$(OBJEXT) : tclWinInt.h
tclWinLoad.$(OBJEXT) : tclWinInt.h
tclWinNotify.$(OBJEXT) : tclInt.h
tclWinPipe.$(OBJEXT) : tclWinInt.h
tclWinReg.$(OBJEXT) : tclInt.h
tclWinSerial.$(OBJEXT) : tclWinInt.h
tclWinSock.$(OBJEXT) : tclWinInt.h
tclWinTest.$(OBJEXT) : tclInt.h
tclWinThrd.$(OBJEXT) : tclWinInt.h
tclWinTime.$(OBJEXT) : tclInt.h

# / end of dependencies.
