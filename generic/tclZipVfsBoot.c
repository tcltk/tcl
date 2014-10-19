#include <tcl.h>
#include "tclInt.h"
#include "tclFileSystem.h"

int Zvfs_Common_Init(Tcl_Interp *);
int Zvfs_Mount(Tcl_Interp *,CONST char *,CONST char *);


/*
** Boot a shell, mount the executable's VFS, detect main.tcl
*/
int Tcl_Zvfs_Boot(const char *archive,const char *vfsmountpoint,const char *initscript) {
  Zvfs_Common_Init(NULL);
  if(!vfsmountpoint) {
    vfsmountpoint="/zvfs";
  }
  if(!initscript) {
    initscript="main.tcl";
  }
  /* We have to initialize the virtual filesystem before calling
  ** Tcl_Init().  Otherwise, Tcl_Init() will not be able to find
  ** its startup script files.
  */
  if(!Zvfs_Mount(NULL, archive, vfsmountpoint)) {
      Tcl_DString filepath;
      Tcl_DString preinit;

      Tcl_Obj *vfsinitscript;
      Tcl_Obj *vfstcllib;
      Tcl_Obj *vfstklib;
      Tcl_Obj *vfspreinit;
      Tcl_DStringInit(&filepath);
      Tcl_DStringInit(&preinit);
          
      Tcl_DStringInit(&filepath);
      Tcl_DStringAppend(&filepath,vfsmountpoint,-1);
      Tcl_DStringAppend(&filepath,"/",-1);
      Tcl_DStringAppend(&filepath,initscript,-1);
      vfsinitscript=Tcl_NewStringObj(Tcl_DStringValue(&filepath),-1);
      Tcl_DStringFree(&filepath);
      
      Tcl_DStringInit(&filepath);
      Tcl_DStringAppend(&filepath,vfsmountpoint,-1);
      Tcl_DStringAppend(&filepath,"/boot/tcl",-1);
      vfstcllib=Tcl_NewStringObj(Tcl_DStringValue(&filepath),-1);
      Tcl_DStringFree(&filepath);

      Tcl_DStringInit(&filepath);
      Tcl_DStringAppend(&filepath,vfsmountpoint,-1);
      Tcl_DStringAppend(&filepath,"/boot/tk",-1);
      vfstklib=Tcl_NewStringObj(Tcl_DStringValue(&filepath),-1);
      Tcl_DStringFree(&filepath);

      Tcl_IncrRefCount(vfsinitscript);
      Tcl_IncrRefCount(vfstcllib);
      Tcl_IncrRefCount(vfstklib);

      if(Tcl_FSAccess(vfsinitscript,F_OK)==0) {
	/* Startup script should be set before calling Tcl_AppInit */
        Tcl_SetStartupScript(vfsinitscript,NULL);
      }
      /* Record the mountpoint for scripts to refer back to */
      Tcl_DStringAppend(&preinit,"\nset ::tcl_boot_vfs ",-1);
      Tcl_DStringAppendElement(&preinit,vfsmountpoint);
      Tcl_DStringAppend(&preinit,"\nset ::SRCDIR ",-1);
      Tcl_DStringAppendElement(&preinit,vfsmountpoint);
      
      if(Tcl_FSAccess(vfstcllib,F_OK)==0) {
        Tcl_DStringAppend(&preinit,"\nset tcl_library ",-1);
        Tcl_DStringAppendElement(&preinit,Tcl_GetString(vfstcllib));
      }
      if(Tcl_FSAccess(vfstklib,F_OK)==0) {
        Tcl_DStringAppend(&preinit,"\nset tk_library ",-1);
        Tcl_DStringAppendElement(&preinit,Tcl_GetString(vfstklib));
      }
      vfspreinit=Tcl_NewStringObj(Tcl_DStringValue(&preinit),-1);
      /* NOTE: We never decr this refcount, lest the contents of the script be deallocated */
      Tcl_IncrRefCount(vfspreinit);
      TclSetPreInitScript(Tcl_GetString(vfspreinit));

      Tcl_DecrRefCount(vfsinitscript);
      Tcl_DecrRefCount(vfstcllib);
      Tcl_DecrRefCount(vfstklib);
  }
  return TCL_OK;
}
