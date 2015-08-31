LOCAL_PATH := $(call my-dir)

###########################
#
# Tcl shared library
#
###########################

include $(CLEAR_VARS)

tcl_path := $(LOCAL_PATH)

include $(tcl_path)/tcl-config.mk

LOCAL_ADDITIONAL_DEPENDENCIES += $(tcl_path)/tcl-config.mk

LOCAL_MODULE := tcl

LOCAL_ARM_MODE := arm

LOCAL_C_INCLUDES := $(tcl_includes) $(LOCAL_PATH)/libtommath

LOCAL_EXPORT_C_INCLUDES := $(LOCAL_C_INCLUDES)

LOCAL_SRC_FILES := \
        libtommath/bncore.c \
        libtommath/bn_reverse.c \
        libtommath/bn_fast_s_mp_mul_digs.c \
        libtommath/bn_fast_s_mp_sqr.c \
        libtommath/bn_mp_add.c \
        libtommath/bn_mp_add_d.c \
        libtommath/bn_mp_and.c \
        libtommath/bn_mp_clamp.c \
        libtommath/bn_mp_clear.c \
        libtommath/bn_mp_clear_multi.c \
        libtommath/bn_mp_cmp.c \
        libtommath/bn_mp_cmp_d.c \
        libtommath/bn_mp_cmp_mag.c \
        libtommath/bn_mp_copy.c \
        libtommath/bn_mp_cnt_lsb.c \
        libtommath/bn_mp_count_bits.c \
        libtommath/bn_mp_div.c \
        libtommath/bn_mp_div_d.c \
        libtommath/bn_mp_div_2.c \
        libtommath/bn_mp_div_2d.c \
        libtommath/bn_mp_div_3.c \
        libtommath/bn_mp_exch.c \
        libtommath/bn_mp_expt_d.c \
        libtommath/bn_mp_grow.c \
        libtommath/bn_mp_init.c \
        libtommath/bn_mp_init_copy.c \
        libtommath/bn_mp_init_multi.c \
        libtommath/bn_mp_init_set.c \
        libtommath/bn_mp_init_set_int.c \
        libtommath/bn_mp_init_size.c \
        libtommath/bn_mp_karatsuba_mul.c \
        libtommath/bn_mp_karatsuba_sqr.c \
        libtommath/bn_mp_lshd.c \
        libtommath/bn_mp_mod.c \
        libtommath/bn_mp_mod_2d.c \
        libtommath/bn_mp_mul.c \
        libtommath/bn_mp_mul_2.c \
        libtommath/bn_mp_mul_2d.c \
        libtommath/bn_mp_mul_d.c \
        libtommath/bn_mp_neg.c \
        libtommath/bn_mp_or.c \
        libtommath/bn_mp_radix_size.c \
        libtommath/bn_mp_radix_smap.c \
        libtommath/bn_mp_read_radix.c \
        libtommath/bn_mp_rshd.c \
        libtommath/bn_mp_set.c \
        libtommath/bn_mp_set_int.c \
        libtommath/bn_mp_shrink.c \
        libtommath/bn_mp_sqr.c \
        libtommath/bn_mp_sqrt.c \
        libtommath/bn_mp_sub.c \
        libtommath/bn_mp_sub_d.c \
        libtommath/bn_mp_to_unsigned_bin.c \
        libtommath/bn_mp_to_unsigned_bin_n.c \
        libtommath/bn_mp_toom_mul.c \
        libtommath/bn_mp_toom_sqr.c \
        libtommath/bn_mp_toradix_n.c \
        libtommath/bn_mp_unsigned_bin_size.c \
        libtommath/bn_mp_xor.c \
        libtommath/bn_mp_zero.c \
        libtommath/bn_s_mp_add.c \
        libtommath/bn_s_mp_mul_digs.c \
        libtommath/bn_s_mp_sqr.c \
        libtommath/bn_s_mp_sub.c \
        generic/regcomp.c \
        generic/regexec.c \
        generic/regfree.c \
        generic/regerror.c \
        generic/tclAlloc.c \
        generic/tclAssembly.c \
        generic/tclAsync.c \
        generic/tclBasic.c \
        generic/tclBinary.c \
        generic/tclCkalloc.c \
        generic/tclClock.c \
        generic/tclCmdAH.c \
        generic/tclCmdIL.c \
        generic/tclCmdMZ.c \
        generic/tclCompCmds.c \
        generic/tclCompCmdsGR.c \
        generic/tclCompCmdsSZ.c \
        generic/tclCompExpr.c \
        generic/tclCompile.c \
        generic/tclConfig.c \
        generic/tclDate.c \
        generic/tclDictObj.c \
        generic/tclDisassemble.c \
        generic/tclEncoding.c \
        generic/tclEnsemble.c \
        generic/tclEnv.c \
        generic/tclEvent.c \
        generic/tclExecute.c \
        generic/tclFCmd.c \
        generic/tclFileName.c \
        generic/tclGet.c \
        generic/tclHash.c \
        generic/tclHistory.c \
        generic/tclIndexObj.c \
        generic/tclInterp.c \
        generic/tclIO.c \
        generic/tclIOCmd.c \
        generic/tclIOGT.c \
        generic/tclIOSock.c \
        generic/tclIOUtil.c \
        generic/tclIORChan.c \
        generic/tclIORTrans.c \
        generic/tclLink.c \
        generic/tclListObj.c \
        generic/tclLiteral.c \
        generic/tclLoad.c \
        generic/tclMain.c \
        generic/tclNamesp.c \
        generic/tclNotify.c \
        generic/tclObj.c \
        generic/tclOptimize.c \
        generic/tclPanic.c \
        generic/tclParse.c \
        generic/tclPathObj.c \
        generic/tclPipe.c \
        generic/tclPkg.c \
        generic/tclPkgConfig.c \
        generic/tclPosixStr.c \
        generic/tclPreserve.c \
        generic/tclProc.c \
        generic/tclRegexp.c \
        generic/tclResolve.c \
        generic/tclResult.c \
        generic/tclScan.c \
        generic/tclStubInit.c \
        generic/tclStringObj.c \
        generic/tclStrToD.c \
        generic/tclThread.c \
        generic/tclThreadAlloc.c \
        generic/tclThreadJoin.c \
        generic/tclThreadStorage.c \
        generic/tclTimer.c \
        generic/tclTomMathInterface.c \
        generic/tclTrace.c \
        generic/tclUtil.c \
        generic/tclUtf.c \
        generic/tclVar.c \
        generic/tclZlib.c \
        generic/tclOO.c \
        generic/tclOOBasic.c \
        generic/tclOOCall.c \
        generic/tclOODefineCmds.c \
        generic/tclOOInfo.c \
        generic/tclOOMethod.c \
        generic/tclOOStubInit.c \
        generic/tclStubLib.c \
        generic/tclTomMathStubLib.c \
        generic/tclOOStubLib.c \
        generic/zipfs.c \
        unix/tclAppInit.c \
        unix/tclLoadDl.c \
        unix/tclUnixChan.c \
        unix/tclUnixCompat.c \
        unix/tclUnixEvent.c \
        unix/tclUnixFCmd.c \
        unix/tclUnixFile.c \
        unix/tclUnixInit.c \
        unix/tclUnixNotfy.c \
        unix/tclUnixPipe.c \
        unix/tclUnixSock.c \
        unix/tclUnixTest.c \
        unix/tclUnixThrd.c \
        unix/tclUnixTime.c

LOCAL_CFLAGS := $(tcl_cflags) \
	-DPACKAGE_NAME="\"tcl\"" \
	-DPACKAGE_VERSION="\"8.6\"" \
	-DBUILD_tcl=1 \
        -Dmain=tclsh \
	-O2

LOCAL_LDLIBS := -ldl -lz -llog

include $(BUILD_SHARED_LIBRARY)

