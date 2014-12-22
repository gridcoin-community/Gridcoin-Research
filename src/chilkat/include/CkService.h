// This header is NOT generated.

#if defined(WIN32) && !defined(WINCE) 

#ifndef _CKSERVICE_H
#define _CKSERVICE_H

#include "CkString.h"
class CkByteData;

/*
    IMPORTANT: Objects returned by methods as non-const pointers must be deleted
    by the calling application. 

  */

#include "CkMultiByteBase.h"

#ifndef __sun__
#pragma pack (push, 8)
#endif
 

// CLASS: CkService
class CkService  : public CkMultiByteBase
{
    private:

	// Don't allow assignment or copying these objects.
	CkService(const CkService &) { } 
	CkService &operator=(const CkService &) { return *this; }
	CkService(void *impl);

    public:
	CkService();
	virtual ~CkService();

	// BEGIN PUBLIC INTERFACE

    	// INSTALL_BEGIN
	bool Install(void);
	// INSTALL_END
	// UNINSTALL_BEGIN
	bool Uninstall(void);
	// UNINSTALL_END
	// START_BEGIN
	bool Start(void);
	// START_END
	// STOP_BEGIN
	bool Stop(void);
	// STOP_END
	// SERVICENAME_BEGIN
	void get_ServiceName(CkString &str);
	const char *serviceName(void);
	void put_ServiceName(const char *newVal);
	// SERVICENAME_END
	// DISPLAYNAME_BEGIN
	void get_DisplayName(CkString &str);
	const char *displayName(void);
	void put_DisplayName(const char *newVal);
	// DISPLAYNAME_END
	// EXEFILENAME_BEGIN
	void get_ExeFilename(CkString &str);
	const char *exeFilename(void);
	void put_ExeFilename(const char *newVal);
	// EXEFILENAME_END
	// AUTOSTART_BEGIN
	bool get_AutoStart(void);
	void put_AutoStart(bool newVal);
	// AUTOSTART_END
	// SETAUTOSTART_BEGIN
	bool SetAutoStart(void);
	// SETAUTOSTART_END
	// SETDEMANDSTART_BEGIN
	bool SetDemandStart(void);
	// SETDEMANDSTART_END
	// DISABLE_BEGIN
	bool Disable(void);
	// DISABLE_END
	// ISAUTOSTART_BEGIN
	int IsAutoStart(void);
	// ISAUTOSTART_END
	// ISDEMANDSTART_BEGIN
	int IsDemandStart(void);
	// ISDEMANDSTART_END
	// ISDISABLED_BEGIN
	int IsDisabled(void);
	// ISDISABLED_END
	// ISINSTALLED_BEGIN
	int IsInstalled(void);
	// ISINSTALLED_END
	// ISRUNNING_BEGIN
	int IsRunning(void);
	// ISRUNNING_END

	// SERVICE_INSERT_POINT

	// END PUBLIC INTERFACE
		
	



};
#ifndef __sun__
#pragma pack (pop)
#endif



#endif  // _CKSERVICE_H


#endif
