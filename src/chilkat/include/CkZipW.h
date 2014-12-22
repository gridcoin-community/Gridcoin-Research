// CkZipW.h: interface for the CkZipW class.
//
//////////////////////////////////////////////////////////////////////

// This header is generated for Chilkat v9.5.0

#ifndef _CkZipW_H
#define _CkZipW_H
	
#include "chilkatDefs.h"

#include "CkString.h"
#include "CkWideCharBase.h"

class CkZipEntryW;
class CkByteData;
class CkStringArrayW;
class CkZipProgressW;



#ifndef __sun__
#pragma pack (push, 8)
#endif
 

// CLASS: CkZipW
class CK_VISIBLE_PUBLIC CkZipW  : public CkWideCharBase
{
    private:
	bool m_cbOwned;
	CkZipProgressW *m_callback;

	// Don't allow assignment or copying these objects.
	CkZipW(const CkZipW &);
	CkZipW &operator=(const CkZipW &);

    public:
	CkZipW(void);
	virtual ~CkZipW(void);

	static CkZipW *createNew(void);
	

	CkZipW(bool bCallbackOwned);
	static CkZipW *createNew(bool bCallbackOwned);

	
	void CK_VISIBLE_PRIVATE inject(void *impl);

	// May be called when finished with the object to free/dispose of any
	// internal resources held by the object. 
	void dispose(void);

	CkZipProgressW *get_EventCallbackObject(void) const;
	void put_EventCallbackObject(CkZipProgressW *progress);


	// BEGIN PUBLIC INTERFACE

	// ----------------------
	// Properties
	// ----------------------
	// When files are added to a Zip archive, they are appended from this directory.
	// For example, if you wish to add all the files under c:/abc/123/myAppDir, you
	// might set this property equal to "c:/abc/123", and then pass "myAppDir/*" to
	// AppendFiles.
	void get_AppendFromDir(CkString &str);
	// When files are added to a Zip archive, they are appended from this directory.
	// For example, if you wish to add all the files under c:/abc/123/myAppDir, you
	// might set this property equal to "c:/abc/123", and then pass "myAppDir/*" to
	// AppendFiles.
	const wchar_t *appendFromDir(void);
	// When files are added to a Zip archive, they are appended from this directory.
	// For example, if you wish to add all the files under c:/abc/123/myAppDir, you
	// might set this property equal to "c:/abc/123", and then pass "myAppDir/*" to
	// AppendFiles.
	void put_AppendFromDir(const wchar_t *newVal);

#if defined(CK_SFX_INCLUDED)
	// (Relevant only when running on a Microsoft Windows operating system.) Optional
	// when creating Windows-based self-extracting EXEs. This is the name of an
	// executable contained within the to-be-created EXE that will automatically be run
	// after extraction. (This is typically something like "setup.exe")
	void get_AutoRun(CkString &str);
	// (Relevant only when running on a Microsoft Windows operating system.) Optional
	// when creating Windows-based self-extracting EXEs. This is the name of an
	// executable contained within the to-be-created EXE that will automatically be run
	// after extraction. (This is typically something like "setup.exe")
	const wchar_t *autoRun(void);
	// (Relevant only when running on a Microsoft Windows operating system.) Optional
	// when creating Windows-based self-extracting EXEs. This is the name of an
	// executable contained within the to-be-created EXE that will automatically be run
	// after extraction. (This is typically something like "setup.exe")
	void put_AutoRun(const wchar_t *newVal);
#endif

#if defined(CK_SFX_INCLUDED)
	// (Relevant only when running on a Microsoft Windows operating system.) Command
	// line parameters that get passed to the AutoRun executable.
	void get_AutoRunParams(CkString &str);
	// (Relevant only when running on a Microsoft Windows operating system.) Command
	// line parameters that get passed to the AutoRun executable.
	const wchar_t *autoRunParams(void);
	// (Relevant only when running on a Microsoft Windows operating system.) Command
	// line parameters that get passed to the AutoRun executable.
	void put_AutoRunParams(const wchar_t *newVal);
#endif

#if defined(CK_SFX_INCLUDED)
	// (Relevant only when running on a Microsoft Windows operating system.) This
	// option applies to creating Windows-based self-extracting EXEs. If true, the
	// to-be-created EXE will automatically select and create a temporary directory for
	// unzipping. This property is often used in conjunction with the AutoRun property
	// to create a self-extracting EXE that automatically unzips to a temp directory
	// and runs a setup.exe without interaction with the user.
	// 
	// Note: To create a self-extracting EXE with no user-interaction, set the
	// following properties to these values:
	// 
	//     ExeSilentProgress = false
	//     ExeNoInterface = true
	//     ExeFinishNotifier = false
	// 
	// The default AutoTemp value is false.
	// 
	bool get_AutoTemp(void);
	// (Relevant only when running on a Microsoft Windows operating system.) This
	// option applies to creating Windows-based self-extracting EXEs. If true, the
	// to-be-created EXE will automatically select and create a temporary directory for
	// unzipping. This property is often used in conjunction with the AutoRun property
	// to create a self-extracting EXE that automatically unzips to a temp directory
	// and runs a setup.exe without interaction with the user.
	// 
	// Note: To create a self-extracting EXE with no user-interaction, set the
	// following properties to these values:
	// 
	//     ExeSilentProgress = false
	//     ExeNoInterface = true
	//     ExeFinishNotifier = false
	// 
	// The default AutoTemp value is false.
	// 
	void put_AutoTemp(bool newVal);
#endif

	// If true then all methods that get or search for zip entries by name will use
	// case-sensitive filename matching. If false then filename matching will be case
	// insensitive. Methods affected by this property include GetEntryByName,
	// UnzipMatching, FirstMatchingEntry, etc.
	// 
	// The default value is true.
	// 
	bool get_CaseSensitive(void);
	// If true then all methods that get or search for zip entries by name will use
	// case-sensitive filename matching. If false then filename matching will be case
	// insensitive. Methods affected by this property include GetEntryByName,
	// UnzipMatching, FirstMatchingEntry, etc.
	// 
	// The default value is true.
	// 
	void put_CaseSensitive(bool newVal);

	// Set this to true to clear the FILE_ATTRIBUTE_ARCHIVE file attribute of each
	// file during a zipping operation.
	// 
	// The default value is false.
	// 
	bool get_ClearArchiveAttribute(void);
	// Set this to true to clear the FILE_ATTRIBUTE_ARCHIVE file attribute of each
	// file during a zipping operation.
	// 
	// The default value is false.
	// 
	void put_ClearArchiveAttribute(bool newVal);

	// If true, the read-only attribute is automatically cleared when unzipping. The
	// default value of this property is false, which leaves the read-only attribute
	// unchanged when unzipping.
	bool get_ClearReadOnlyAttr(void);
	// If true, the read-only attribute is automatically cleared when unzipping. The
	// default value of this property is false, which leaves the read-only attribute
	// unchanged when unzipping.
	void put_ClearReadOnlyAttr(bool newVal);

	// The global Zip file comment.
	void get_Comment(CkString &str);
	// The global Zip file comment.
	const wchar_t *comment(void);
	// The global Zip file comment.
	void put_Comment(const wchar_t *newVal);

	// When opening a password-protected or AES encrypted Zip, this is the password to
	// be used for decryption. Encrypted Zips may be opened without setting a password,
	// but the contents cannot be unzipped without setting this password.
	// 
	// Note:The SetPassword method has the effect of setting both this property as well
	// as the EncryptPassword property. The SetPassword method should no longer be
	// used. It has been replaced by the DecryptPassword and EncryptPassword properties
	// to make it possible to open an encrypted zip and re-write it with a new
	// password.
	// 
	void get_DecryptPassword(CkString &str);
	// When opening a password-protected or AES encrypted Zip, this is the password to
	// be used for decryption. Encrypted Zips may be opened without setting a password,
	// but the contents cannot be unzipped without setting this password.
	// 
	// Note:The SetPassword method has the effect of setting both this property as well
	// as the EncryptPassword property. The SetPassword method should no longer be
	// used. It has been replaced by the DecryptPassword and EncryptPassword properties
	// to make it possible to open an encrypted zip and re-write it with a new
	// password.
	// 
	const wchar_t *decryptPassword(void);
	// When opening a password-protected or AES encrypted Zip, this is the password to
	// be used for decryption. Encrypted Zips may be opened without setting a password,
	// but the contents cannot be unzipped without setting this password.
	// 
	// Note:The SetPassword method has the effect of setting both this property as well
	// as the EncryptPassword property. The SetPassword method should no longer be
	// used. It has been replaced by the DecryptPassword and EncryptPassword properties
	// to make it possible to open an encrypted zip and re-write it with a new
	// password.
	// 
	void put_DecryptPassword(const wchar_t *newVal);

	// If true, discards all file path information when zipping. The default value is
	// false.
	bool get_DiscardPaths(void);
	// If true, discards all file path information when zipping. The default value is
	// false.
	void put_DiscardPaths(bool newVal);

	// The encryption key length if AES, Blowfish, Twofish, or WinZip-compatible AES
	// encryption is used. This value must be 128, 192, or 256. The default value is
	// 128.
	int get_EncryptKeyLength(void);
	// The encryption key length if AES, Blowfish, Twofish, or WinZip-compatible AES
	// encryption is used. This value must be 128, 192, or 256. The default value is
	// 128.
	void put_EncryptKeyLength(int newVal);

	// The password used when writing a password-protected or strong-encrytped Zip.
	// 
	// Note:The SetPassword method has the effect of setting both this property as well
	// as the DecryptPassword property. The SetPassword method should no longer be
	// used. It has been replaced by the DecryptPassword and EncryptPassword properties
	// to make it possible to open an encrypted zip and re-write it with a new
	// password.
	// 
	void get_EncryptPassword(CkString &str);
	// The password used when writing a password-protected or strong-encrytped Zip.
	// 
	// Note:The SetPassword method has the effect of setting both this property as well
	// as the DecryptPassword property. The SetPassword method should no longer be
	// used. It has been replaced by the DecryptPassword and EncryptPassword properties
	// to make it possible to open an encrypted zip and re-write it with a new
	// password.
	// 
	const wchar_t *encryptPassword(void);
	// The password used when writing a password-protected or strong-encrytped Zip.
	// 
	// Note:The SetPassword method has the effect of setting both this property as well
	// as the DecryptPassword property. The SetPassword method should no longer be
	// used. It has been replaced by the DecryptPassword and EncryptPassword properties
	// to make it possible to open an encrypted zip and re-write it with a new
	// password.
	// 
	void put_EncryptPassword(const wchar_t *newVal);

	// Indicate whether the Zip is to be strong encrypted or not. Valid values are 0
	// (not encrypted) or 4 (AES encrypted). When this property is set to the value 4,
	// WinZip AES compatible encrypted zip archives are produced.
	// 
	// Note: Prior to Chilkat v9.4.1, other possible values for this property were: 1
	// (blowfish), 2 (twofish), and 3 (rijndael). These settings originally provided a
	// way to produce strong encrypted zips prior to when the AES encrypted Zip
	// standard existed. Using these legacy values (1, 2, or 3) produced encrypted zips
	// that only applications using Chilkat could read. Chilkat no longer supports
	// these custom modes of encryption. If using an older version of Chilkat with one
	// of these deprecated encryption modes, make sure to decrypt using the old Chilkat
	// version and re-encrypt using mode 4 (WinZip compatible AES encryption) prior to
	// updating to the new Chilkat version.
	// 
	// Important:The Encryption and PasswordProtect properties are mutually exclusive.
	// PasswordProtect corresponds to the older Zip 2.0 encryption, commonly referred
	// to as a "password-protected" zip. If the PasswordProtect is set to true, the
	// Encryption property should be set to 0. If the Encryption property is set to a
	// non-zero value, then PasswordProtect should be set to false. A zip cannot be
	// both password-protected and strong-encrypted.
	// 
	int get_Encryption(void);
	// Indicate whether the Zip is to be strong encrypted or not. Valid values are 0
	// (not encrypted) or 4 (AES encrypted). When this property is set to the value 4,
	// WinZip AES compatible encrypted zip archives are produced.
	// 
	// Note: Prior to Chilkat v9.4.1, other possible values for this property were: 1
	// (blowfish), 2 (twofish), and 3 (rijndael). These settings originally provided a
	// way to produce strong encrypted zips prior to when the AES encrypted Zip
	// standard existed. Using these legacy values (1, 2, or 3) produced encrypted zips
	// that only applications using Chilkat could read. Chilkat no longer supports
	// these custom modes of encryption. If using an older version of Chilkat with one
	// of these deprecated encryption modes, make sure to decrypt using the old Chilkat
	// version and re-encrypt using mode 4 (WinZip compatible AES encryption) prior to
	// updating to the new Chilkat version.
	// 
	// Important:The Encryption and PasswordProtect properties are mutually exclusive.
	// PasswordProtect corresponds to the older Zip 2.0 encryption, commonly referred
	// to as a "password-protected" zip. If the PasswordProtect is set to true, the
	// Encryption property should be set to 0. If the Encryption property is set to a
	// non-zero value, then PasswordProtect should be set to false. A zip cannot be
	// both password-protected and strong-encrypted.
	// 
	void put_Encryption(int newVal);

#if defined(CK_SFX_INCLUDED)
	// (Relevant only when running on a Microsoft Windows operating system.) Specifies
	// the default unzip directory path to appear in the user-interface dialog box when
	// the Windows-based self-extracting EXE is run.
	void get_ExeDefaultDir(CkString &str);
	// (Relevant only when running on a Microsoft Windows operating system.) Specifies
	// the default unzip directory path to appear in the user-interface dialog box when
	// the Windows-based self-extracting EXE is run.
	const wchar_t *exeDefaultDir(void);
	// (Relevant only when running on a Microsoft Windows operating system.) Specifies
	// the default unzip directory path to appear in the user-interface dialog box when
	// the Windows-based self-extracting EXE is run.
	void put_ExeDefaultDir(const wchar_t *newVal);
#endif

#if defined(CK_SFX_INCLUDED)
	// (Relevant only when running on a Microsoft Windows operating system.) If set to
	// true, a "Finished" dialog box is displayed when the self-extracting EXE is
	// finished extracting. The caption, title, and button text of the finish notifier
	// dialog may be customized by calling SetExeConfigParam. The default value is
	// false.
	bool get_ExeFinishNotifier(void);
	// (Relevant only when running on a Microsoft Windows operating system.) If set to
	// true, a "Finished" dialog box is displayed when the self-extracting EXE is
	// finished extracting. The caption, title, and button text of the finish notifier
	// dialog may be customized by calling SetExeConfigParam. The default value is
	// false.
	void put_ExeFinishNotifier(bool newVal);
#endif

#if defined(CK_SFX_INCLUDED)
	// (Relevant only when running on a Microsoft Windows operating system.) Applies to
	// creating self-extracting EXEs. This property can be set to a pre-existing icon
	// filename (.ico) that will be embedded within the to-be-created EXE and set as
	// its default icon.
	void get_ExeIconFile(CkString &str);
	// (Relevant only when running on a Microsoft Windows operating system.) Applies to
	// creating self-extracting EXEs. This property can be set to a pre-existing icon
	// filename (.ico) that will be embedded within the to-be-created EXE and set as
	// its default icon.
	const wchar_t *exeIconFile(void);
	// (Relevant only when running on a Microsoft Windows operating system.) Applies to
	// creating self-extracting EXEs. This property can be set to a pre-existing icon
	// filename (.ico) that will be embedded within the to-be-created EXE and set as
	// its default icon.
	void put_ExeIconFile(const wchar_t *newVal);
#endif

#if defined(CK_SFX_INCLUDED)
	// (Relevant only when running on a Microsoft Windows operating system.) Applies to
	// creating Windows-based self-extracting EXEs. When set to true, the
	// to-be-created EXE will run without a user-interface. The default value is
	// false.
	// 
	// Note: The ExeSilentProgress property needs to be set to true for the extract
	// to be truly silent.
	// 
	// Important: If the AutoTemp property = true and there is no AutoRun EXE, and
	// there is no ExeUnzipDir set, then the self-extracting EXE will always display a
	// dialog to get the unzip directory. The reason is that it makes no sense to
	// silently unzip to an auto-selected (and unknown) temp directory without anything
	// happening afterwards.
	// 
	// Important: If the self-extracting EXE is encrypted, a password dialog will be
	// displayed. The password dialog may be suppressed if the password is provided on
	// the command line via the -pwd command-line option.
	// 
	bool get_ExeNoInterface(void);
	// (Relevant only when running on a Microsoft Windows operating system.) Applies to
	// creating Windows-based self-extracting EXEs. When set to true, the
	// to-be-created EXE will run without a user-interface. The default value is
	// false.
	// 
	// Note: The ExeSilentProgress property needs to be set to true for the extract
	// to be truly silent.
	// 
	// Important: If the AutoTemp property = true and there is no AutoRun EXE, and
	// there is no ExeUnzipDir set, then the self-extracting EXE will always display a
	// dialog to get the unzip directory. The reason is that it makes no sense to
	// silently unzip to an auto-selected (and unknown) temp directory without anything
	// happening afterwards.
	// 
	// Important: If the self-extracting EXE is encrypted, a password dialog will be
	// displayed. The password dialog may be suppressed if the password is provided on
	// the command line via the -pwd command-line option.
	// 
	void put_ExeNoInterface(bool newVal);
#endif

#if defined(CK_SFX_INCLUDED)
	// (Relevant only when running on a Microsoft Windows operating system.) Determines
	// whether a progress dialog is displayed when the self-extracting EXE is run. If
	// ExeNoInterface = false (i.e. there is a main dialog with the ability to select
	// the unzip directory), then the progress dialog is (by default) shown as a
	// progress bar within the main dialog -- and this property has no effect. If
	// ExeNoInterface = true, then a progress-only dialog is displayed if
	// ExeSilentProgress = false. The default value of ExeSilentProgress is true.
	bool get_ExeSilentProgress(void);
	// (Relevant only when running on a Microsoft Windows operating system.) Determines
	// whether a progress dialog is displayed when the self-extracting EXE is run. If
	// ExeNoInterface = false (i.e. there is a main dialog with the ability to select
	// the unzip directory), then the progress dialog is (by default) shown as a
	// progress bar within the main dialog -- and this property has no effect. If
	// ExeNoInterface = true, then a progress-only dialog is displayed if
	// ExeSilentProgress = false. The default value of ExeSilentProgress is true.
	void put_ExeSilentProgress(bool newVal);
#endif

#if defined(CK_SFX_INCLUDED)
	// (Relevant only when running on a Microsoft Windows operating system.) Applies to
	// creating Windows-based self-extracting EXEs. Sets the title of the main
	// user-interface dialog that appears when the self-extracting EXE runs.
	void get_ExeTitle(CkString &str);
	// (Relevant only when running on a Microsoft Windows operating system.) Applies to
	// creating Windows-based self-extracting EXEs. Sets the title of the main
	// user-interface dialog that appears when the self-extracting EXE runs.
	const wchar_t *exeTitle(void);
	// (Relevant only when running on a Microsoft Windows operating system.) Applies to
	// creating Windows-based self-extracting EXEs. Sets the title of the main
	// user-interface dialog that appears when the self-extracting EXE runs.
	void put_ExeTitle(const wchar_t *newVal);
#endif

#if defined(CK_SFX_INCLUDED)
	// (Relevant only when running on a Microsoft Windows operating system.) Applies to
	// creating MS Windows-based self-extracting EXEs. Sets the unzipping caption of
	// the main user-interface dialog that appears when the self-extracting EXE runs.
	void get_ExeUnzipCaption(CkString &str);
	// (Relevant only when running on a Microsoft Windows operating system.) Applies to
	// creating MS Windows-based self-extracting EXEs. Sets the unzipping caption of
	// the main user-interface dialog that appears when the self-extracting EXE runs.
	const wchar_t *exeUnzipCaption(void);
	// (Relevant only when running on a Microsoft Windows operating system.) Applies to
	// creating MS Windows-based self-extracting EXEs. Sets the unzipping caption of
	// the main user-interface dialog that appears when the self-extracting EXE runs.
	void put_ExeUnzipCaption(const wchar_t *newVal);
#endif

#if defined(CK_SFX_INCLUDED)
	// (Relevant only when running on a Microsoft Windows operating system.) Applies to
	// creating MS Windows self-extracting EXEs. Stores a pre-defined unzip directory
	// within the self-extracting EXE so that it automatically unzips to this directory
	// without user-intervention.
	// 
	// Environment variables may be included if surrounded by percent characters. For
	// example: %TEMP%. Environment variables are expanded (i.e. resolved) when the
	// self-extracting EXE runs.
	// 
	// Note: To create a self-extracting EXE with no user-interaction, set the
	// following properties to these values:
	// 
	//     ExeSilentProgress = false
	//     ExeNoInterface = true
	//     ExeFinishNotifier = false
	// 
	void get_ExeUnzipDir(CkString &str);
	// (Relevant only when running on a Microsoft Windows operating system.) Applies to
	// creating MS Windows self-extracting EXEs. Stores a pre-defined unzip directory
	// within the self-extracting EXE so that it automatically unzips to this directory
	// without user-intervention.
	// 
	// Environment variables may be included if surrounded by percent characters. For
	// example: %TEMP%. Environment variables are expanded (i.e. resolved) when the
	// self-extracting EXE runs.
	// 
	// Note: To create a self-extracting EXE with no user-interaction, set the
	// following properties to these values:
	// 
	//     ExeSilentProgress = false
	//     ExeNoInterface = true
	//     ExeFinishNotifier = false
	// 
	const wchar_t *exeUnzipDir(void);
	// (Relevant only when running on a Microsoft Windows operating system.) Applies to
	// creating MS Windows self-extracting EXEs. Stores a pre-defined unzip directory
	// within the self-extracting EXE so that it automatically unzips to this directory
	// without user-intervention.
	// 
	// Environment variables may be included if surrounded by percent characters. For
	// example: %TEMP%. Environment variables are expanded (i.e. resolved) when the
	// self-extracting EXE runs.
	// 
	// Note: To create a self-extracting EXE with no user-interaction, set the
	// following properties to these values:
	// 
	//     ExeSilentProgress = false
	//     ExeNoInterface = true
	//     ExeFinishNotifier = false
	// 
	void put_ExeUnzipDir(const wchar_t *newVal);
#endif

#if defined(CK_SFX_INCLUDED)
	// (Relevant only when running on a Microsoft Windows operating system.) If true,
	// the self-extracting EXE will wait for the AutoRun EXE to complete before it
	// exits. If false, the self-extracting EXE dialog (or process if running
	// silently with no user-interface), is allowed to exit prior to the completion of
	// the AutoRun EXE. The default value is true.
	bool get_ExeWaitForSetup(void);
	// (Relevant only when running on a Microsoft Windows operating system.) If true,
	// the self-extracting EXE will wait for the AutoRun EXE to complete before it
	// exits. If false, the self-extracting EXE dialog (or process if running
	// silently with no user-interface), is allowed to exit prior to the completion of
	// the AutoRun EXE. The default value is true.
	void put_ExeWaitForSetup(bool newVal);
#endif

#if defined(CK_SFX_INCLUDED)
	// (Relevant only when running on a Microsoft Windows operating system.) Allows for
	// an XML config document to be used to specify all possible options for
	// self-extracting EXEs. This property is a string containing the XML config
	// document.
	// 
	// The XML should have this format:
	// _LT_SfxConfig_GT_
	// 	_LT_ErrPwdTitle_GT_Title for incorrect password dialog_LT_/ErrPwdTitle_GT_
	// 	_LT_ErrPwdCaption_GT_Caption for incorrect password dialog_LT_/ErrPwdCaption_GT_
	// 	_LT_FinOkBtn_GT_Text on finish notifier button_LT_/FinOkBtn_GT_
	// 	_LT_PwdOkBtn_GT_Text on password challenge dialog's _QUOTE_OK_QUOTE_ button._LT_/PwdOkBtn_GT_
	// 	_LT_PwdCancelBtn_GT_Text on password challenge dialog's Cancel button._LT_/PwdCancelBtn_GT_
	// 	_LT_ErrInvalidPassword_GT_Incorrect password error message._LT_/ErrInvalidPassword_GT_
	// 	_LT_MainUnzipBtn_GT_Text on main dialog's unzip button_LT_/MainUnzipBtn_GT_
	// 	_LT_MainCloseBtn_GT_Text on main dialog's quit/exit button_LT_/MainCloseBtn_GT_
	// 	_LT_MainBrowseBtn_GT_Text on main dialog's browse-for-directory button._LT_/MainBrowseBtn_GT_
	// 	_LT_MainUnzipLabel_GT_Caption displayed in main dialog._LT_/MainUnzipLabel_GT_
	// 	_LT_AutoTemp_GT__QUOTE_1|0 (Maps to the AutoTemp property)_QUOTE__LT_/AutoTemp_GT_
	// 	_LT_Cleanup_GT__QUOTE_1|0 (Deletes extracted files after the SetupExe is run.)_QUOTE__LT_/Cleanup_GT_
	// 	_LT_Debug_GT__QUOTE_1|0  (If 1, the EXE will not extract any files.)_QUOTE__LT_/Debug_GT_
	// 	_LT_Verbose_GT__QUOTE_1|0 (If 1, then verbose information is sent to the log.)_QUOTE__LT_/Verbose_GT_
	// 	_LT_ShowFin_GT__QUOTE_1|0_QUOTE_ Maps to ExeFinishNotifier property._LT_/ShowFin_GT_
	// 	_LT_ShowMain_GT__QUOTE_1|0_QUOTE_ Maps to ExeNoInterface property._LT_/ShowMain_GT_
	// 	_LT_ShowProgress_GT__QUOTE_1|0_QUOTE_ Maps to ExeSilentProgress property._LT_/ShowProgress_GT_
	// 	_LT_WaitForSetup_GT__QUOTE_1|0_QUOTE_ Maps to ExeWaitForSetup property._LT_/WaitForSetup_GT_
	// 	_LT_Encryption_GT__QUOTE_1|0_QUOTE_  1=Yes, 0=No_LT_/Encryption_GT_
	// 	_LT_KeyLength_GT_128|192|256_LT_/KeyLength_GT_
	// 	_LT_SetupExe_GT_EXE to run after extracting. (Maps to AutoRun property)_LT_/SetupExe_GT_
	// 	_LT_UnzipDir_GT_Pre-defined unzip directory. (Maps to ExeUnzipDir property)_GT_
	// 	_LT_DefaultDir_GT_Default unzip directory to appear in the main dialog. 
	//                                                 (Maps to ExeDefaultDir property)_LT_/DefaultDir_GT_
	// 	_LT_IconFile_GT_Icon file to be used (Maps to ExeIconFile property)_LT_/IconFile_GT_
	// 	_LT_Url_GT_Maps to ExeSourceUrl property._LT_/Url_GT_
	// 	_LT_MainTitle_GT_Maps to ExeTitle property._LT_/MainTitle_GT_
	// 	_LT_MainCaption_GT_Maps to ExeUnzipCaption property._LT_/MainCaption_GT_
	// 	_LT_FinTitle_GT_Title for the finish notifier dialog._LT_/FinTitle_GT_
	// 	_LT_FinCaption_GT_Caption for the finish notifier dialog._LT_/FinTitle_GT_
	// 	_LT_ProgressTitle_GT_Title for the progress dialog._LT_/ProgressTitle_GT_
	// 	_LT_ProgressCaption_GT_Caption for the progress dialog._LT_/ProgressCaption_GT_
	// 	_LT_PwTitle_GT_Title for the password challenge dialog._LT_/PwTitle_GT_
	// 	_LT_PwCaption_GT_Caption for the password challenge dialog._LT_/PwCaption_GT_
	// _LT_/SfxConfig_GT_
	// 
	// A self-extracting EXE can be run from the command line with the "-log
	// {logFilePath}" option to create a log with information for debugging.
	// 
	void get_ExeXmlConfig(CkString &str);
	// (Relevant only when running on a Microsoft Windows operating system.) Allows for
	// an XML config document to be used to specify all possible options for
	// self-extracting EXEs. This property is a string containing the XML config
	// document.
	// 
	// The XML should have this format:
	// _LT_SfxConfig_GT_
	// 	_LT_ErrPwdTitle_GT_Title for incorrect password dialog_LT_/ErrPwdTitle_GT_
	// 	_LT_ErrPwdCaption_GT_Caption for incorrect password dialog_LT_/ErrPwdCaption_GT_
	// 	_LT_FinOkBtn_GT_Text on finish notifier button_LT_/FinOkBtn_GT_
	// 	_LT_PwdOkBtn_GT_Text on password challenge dialog's _QUOTE_OK_QUOTE_ button._LT_/PwdOkBtn_GT_
	// 	_LT_PwdCancelBtn_GT_Text on password challenge dialog's Cancel button._LT_/PwdCancelBtn_GT_
	// 	_LT_ErrInvalidPassword_GT_Incorrect password error message._LT_/ErrInvalidPassword_GT_
	// 	_LT_MainUnzipBtn_GT_Text on main dialog's unzip button_LT_/MainUnzipBtn_GT_
	// 	_LT_MainCloseBtn_GT_Text on main dialog's quit/exit button_LT_/MainCloseBtn_GT_
	// 	_LT_MainBrowseBtn_GT_Text on main dialog's browse-for-directory button._LT_/MainBrowseBtn_GT_
	// 	_LT_MainUnzipLabel_GT_Caption displayed in main dialog._LT_/MainUnzipLabel_GT_
	// 	_LT_AutoTemp_GT__QUOTE_1|0 (Maps to the AutoTemp property)_QUOTE__LT_/AutoTemp_GT_
	// 	_LT_Cleanup_GT__QUOTE_1|0 (Deletes extracted files after the SetupExe is run.)_QUOTE__LT_/Cleanup_GT_
	// 	_LT_Debug_GT__QUOTE_1|0  (If 1, the EXE will not extract any files.)_QUOTE__LT_/Debug_GT_
	// 	_LT_Verbose_GT__QUOTE_1|0 (If 1, then verbose information is sent to the log.)_QUOTE__LT_/Verbose_GT_
	// 	_LT_ShowFin_GT__QUOTE_1|0_QUOTE_ Maps to ExeFinishNotifier property._LT_/ShowFin_GT_
	// 	_LT_ShowMain_GT__QUOTE_1|0_QUOTE_ Maps to ExeNoInterface property._LT_/ShowMain_GT_
	// 	_LT_ShowProgress_GT__QUOTE_1|0_QUOTE_ Maps to ExeSilentProgress property._LT_/ShowProgress_GT_
	// 	_LT_WaitForSetup_GT__QUOTE_1|0_QUOTE_ Maps to ExeWaitForSetup property._LT_/WaitForSetup_GT_
	// 	_LT_Encryption_GT__QUOTE_1|0_QUOTE_  1=Yes, 0=No_LT_/Encryption_GT_
	// 	_LT_KeyLength_GT_128|192|256_LT_/KeyLength_GT_
	// 	_LT_SetupExe_GT_EXE to run after extracting. (Maps to AutoRun property)_LT_/SetupExe_GT_
	// 	_LT_UnzipDir_GT_Pre-defined unzip directory. (Maps to ExeUnzipDir property)_GT_
	// 	_LT_DefaultDir_GT_Default unzip directory to appear in the main dialog. 
	//                                                 (Maps to ExeDefaultDir property)_LT_/DefaultDir_GT_
	// 	_LT_IconFile_GT_Icon file to be used (Maps to ExeIconFile property)_LT_/IconFile_GT_
	// 	_LT_Url_GT_Maps to ExeSourceUrl property._LT_/Url_GT_
	// 	_LT_MainTitle_GT_Maps to ExeTitle property._LT_/MainTitle_GT_
	// 	_LT_MainCaption_GT_Maps to ExeUnzipCaption property._LT_/MainCaption_GT_
	// 	_LT_FinTitle_GT_Title for the finish notifier dialog._LT_/FinTitle_GT_
	// 	_LT_FinCaption_GT_Caption for the finish notifier dialog._LT_/FinTitle_GT_
	// 	_LT_ProgressTitle_GT_Title for the progress dialog._LT_/ProgressTitle_GT_
	// 	_LT_ProgressCaption_GT_Caption for the progress dialog._LT_/ProgressCaption_GT_
	// 	_LT_PwTitle_GT_Title for the password challenge dialog._LT_/PwTitle_GT_
	// 	_LT_PwCaption_GT_Caption for the password challenge dialog._LT_/PwCaption_GT_
	// _LT_/SfxConfig_GT_
	// 
	// A self-extracting EXE can be run from the command line with the "-log
	// {logFilePath}" option to create a log with information for debugging.
	// 
	const wchar_t *exeXmlConfig(void);
	// (Relevant only when running on a Microsoft Windows operating system.) Allows for
	// an XML config document to be used to specify all possible options for
	// self-extracting EXEs. This property is a string containing the XML config
	// document.
	// 
	// The XML should have this format:
	// _LT_SfxConfig_GT_
	// 	_LT_ErrPwdTitle_GT_Title for incorrect password dialog_LT_/ErrPwdTitle_GT_
	// 	_LT_ErrPwdCaption_GT_Caption for incorrect password dialog_LT_/ErrPwdCaption_GT_
	// 	_LT_FinOkBtn_GT_Text on finish notifier button_LT_/FinOkBtn_GT_
	// 	_LT_PwdOkBtn_GT_Text on password challenge dialog's _QUOTE_OK_QUOTE_ button._LT_/PwdOkBtn_GT_
	// 	_LT_PwdCancelBtn_GT_Text on password challenge dialog's Cancel button._LT_/PwdCancelBtn_GT_
	// 	_LT_ErrInvalidPassword_GT_Incorrect password error message._LT_/ErrInvalidPassword_GT_
	// 	_LT_MainUnzipBtn_GT_Text on main dialog's unzip button_LT_/MainUnzipBtn_GT_
	// 	_LT_MainCloseBtn_GT_Text on main dialog's quit/exit button_LT_/MainCloseBtn_GT_
	// 	_LT_MainBrowseBtn_GT_Text on main dialog's browse-for-directory button._LT_/MainBrowseBtn_GT_
	// 	_LT_MainUnzipLabel_GT_Caption displayed in main dialog._LT_/MainUnzipLabel_GT_
	// 	_LT_AutoTemp_GT__QUOTE_1|0 (Maps to the AutoTemp property)_QUOTE__LT_/AutoTemp_GT_
	// 	_LT_Cleanup_GT__QUOTE_1|0 (Deletes extracted files after the SetupExe is run.)_QUOTE__LT_/Cleanup_GT_
	// 	_LT_Debug_GT__QUOTE_1|0  (If 1, the EXE will not extract any files.)_QUOTE__LT_/Debug_GT_
	// 	_LT_Verbose_GT__QUOTE_1|0 (If 1, then verbose information is sent to the log.)_QUOTE__LT_/Verbose_GT_
	// 	_LT_ShowFin_GT__QUOTE_1|0_QUOTE_ Maps to ExeFinishNotifier property._LT_/ShowFin_GT_
	// 	_LT_ShowMain_GT__QUOTE_1|0_QUOTE_ Maps to ExeNoInterface property._LT_/ShowMain_GT_
	// 	_LT_ShowProgress_GT__QUOTE_1|0_QUOTE_ Maps to ExeSilentProgress property._LT_/ShowProgress_GT_
	// 	_LT_WaitForSetup_GT__QUOTE_1|0_QUOTE_ Maps to ExeWaitForSetup property._LT_/WaitForSetup_GT_
	// 	_LT_Encryption_GT__QUOTE_1|0_QUOTE_  1=Yes, 0=No_LT_/Encryption_GT_
	// 	_LT_KeyLength_GT_128|192|256_LT_/KeyLength_GT_
	// 	_LT_SetupExe_GT_EXE to run after extracting. (Maps to AutoRun property)_LT_/SetupExe_GT_
	// 	_LT_UnzipDir_GT_Pre-defined unzip directory. (Maps to ExeUnzipDir property)_GT_
	// 	_LT_DefaultDir_GT_Default unzip directory to appear in the main dialog. 
	//                                                 (Maps to ExeDefaultDir property)_LT_/DefaultDir_GT_
	// 	_LT_IconFile_GT_Icon file to be used (Maps to ExeIconFile property)_LT_/IconFile_GT_
	// 	_LT_Url_GT_Maps to ExeSourceUrl property._LT_/Url_GT_
	// 	_LT_MainTitle_GT_Maps to ExeTitle property._LT_/MainTitle_GT_
	// 	_LT_MainCaption_GT_Maps to ExeUnzipCaption property._LT_/MainCaption_GT_
	// 	_LT_FinTitle_GT_Title for the finish notifier dialog._LT_/FinTitle_GT_
	// 	_LT_FinCaption_GT_Caption for the finish notifier dialog._LT_/FinTitle_GT_
	// 	_LT_ProgressTitle_GT_Title for the progress dialog._LT_/ProgressTitle_GT_
	// 	_LT_ProgressCaption_GT_Caption for the progress dialog._LT_/ProgressCaption_GT_
	// 	_LT_PwTitle_GT_Title for the password challenge dialog._LT_/PwTitle_GT_
	// 	_LT_PwCaption_GT_Caption for the password challenge dialog._LT_/PwCaption_GT_
	// _LT_/SfxConfig_GT_
	// 
	// A self-extracting EXE can be run from the command line with the "-log
	// {logFilePath}" option to create a log with information for debugging.
	// 
	void put_ExeXmlConfig(const wchar_t *newVal);
#endif

	// The number of files (excluding directories) contained within the Zip.
	int get_FileCount(void);

	// The path (absolute or relative) of the Zip archive. This is the path of the file
	// that is created or overwritten when the zip is saved.
	void get_FileName(CkString &str);
	// The path (absolute or relative) of the Zip archive. This is the path of the file
	// that is created or overwritten when the zip is saved.
	const wchar_t *fileName(void);
	// The path (absolute or relative) of the Zip archive. This is the path of the file
	// that is created or overwritten when the zip is saved.
	void put_FileName(const wchar_t *newVal);

	// true if the opened zip contained file format errors (that were not severe
	// enough to prevent the zip from being opened and parsed).
	bool get_HasZipFormatErrors(void);

	// The number of milliseconds between each AbortCheck event callback. The
	// AbortCheck callback allows an application to abort any method call prior to
	// completion. If HeartbeatMs is 0 (the default), no AbortCheck event callbacks
	// will fire.
	int get_HeartbeatMs(void);
	// The number of milliseconds between each AbortCheck event callback. The
	// AbortCheck callback allows an application to abort any method call prior to
	// completion. If HeartbeatMs is 0 (the default), no AbortCheck event callbacks
	// will fire.
	void put_HeartbeatMs(int newVal);

	// If true, then files that cannot be read due to "access denied" (i.e. a file
	// permission error) will be ignored and the call to WriteZip, WriteZipAndClose,
	// WriteExe, etc. will return a success status. If false, then the "access
	// denied" filesystem errors are not ignored and any occurance will cause the zip
	// writing to fail. The default value is true.
	bool get_IgnoreAccessDenied(void);
	// If true, then files that cannot be read due to "access denied" (i.e. a file
	// permission error) will be ignored and the call to WriteZip, WriteZipAndClose,
	// WriteExe, etc. will return a success status. If false, then the "access
	// denied" filesystem errors are not ignored and any occurance will cause the zip
	// writing to fail. The default value is true.
	void put_IgnoreAccessDenied(bool newVal);

	// The number of entries in the Zip, including both files and directories.
	int get_NumEntries(void);

	// Sets the OEM code page to be used for Unicode filenames. This property defaults
	// to the OEM code page of the computer.
	int get_OemCodePage(void);
	// Sets the OEM code page to be used for Unicode filenames. This property defaults
	// to the OEM code page of the computer.
	void put_OemCodePage(int newVal);

	// Determines whether existing files are overwritten during unzipping. The default
	// is true, which means that already-existing files will be overwritten. Set this
	// property = false to prevent existing files from being overwritten when
	// unzipping.
	bool get_OverwriteExisting(void);
	// Determines whether existing files are overwritten during unzipping. The default
	// is true, which means that already-existing files will be overwritten. Set this
	// property = false to prevent existing files from being overwritten when
	// unzipping.
	void put_OverwriteExisting(bool newVal);

	// true if the Zip should be password-protected using older Zip 2.0 encryption,
	// commonly referred to as "password-protection".
	bool get_PasswordProtect(void);
	// true if the Zip should be password-protected using older Zip 2.0 encryption,
	// commonly referred to as "password-protection".
	void put_PasswordProtect(bool newVal);

	// A prefix that is added to each filename when zipping. One might set the
	// PathPrefix to "subdir/" so that files are unzipped to a specified subdirectory
	// when unzipping.
	void get_PathPrefix(CkString &str);
	// A prefix that is added to each filename when zipping. One might set the
	// PathPrefix to "subdir/" so that files are unzipped to a specified subdirectory
	// when unzipping.
	const wchar_t *pathPrefix(void);
	// A prefix that is added to each filename when zipping. One might set the
	// PathPrefix to "subdir/" so that files are unzipped to a specified subdirectory
	// when unzipping.
	void put_PathPrefix(const wchar_t *newVal);

	// The temporary directory to use when unzipping files. When running in ASP or
	// ASP.NET, the default value of TempDir is set to the directory where the .zip is
	// being written. Set this property to override the default.
	void get_TempDir(CkString &str);
	// The temporary directory to use when unzipping files. When running in ASP or
	// ASP.NET, the default value of TempDir is set to the directory where the .zip is
	// being written. Set this property to override the default.
	const wchar_t *tempDir(void);
	// The temporary directory to use when unzipping files. When running in ASP or
	// ASP.NET, the default value of TempDir is set to the directory where the .zip is
	// being written. Set this property to override the default.
	void put_TempDir(const wchar_t *newVal);

	// If set to true, the component will set the "text flag" for each file having
	// these filename extensions: .txt, .xml, .htm, and .html. It will also preserve
	// the "text flag" for existing zips that are opened and rewritten. By default,
	// this property is set to false.
	// 
	// It is generally not necessary to set the text flag for a zip entry.
	// 
	bool get_TextFlag(void);
	// If set to true, the component will set the "text flag" for each file having
	// these filename extensions: .txt, .xml, .htm, and .html. It will also preserve
	// the "text flag" for existing zips that are opened and rewritten. By default,
	// this property is set to false.
	// 
	// It is generally not necessary to set the text flag for a zip entry.
	// 
	void put_TextFlag(bool newVal);

	// Starting in v9.4.1, Chilkat Zip will automatically unzip ZIPX files using any of
	// the following compression methods: BZIP2, PPMd, LZMA, and Deflate64 ("Deflate64"
	// is a trademark of PKWare, Inc.)
	// 
	// This property, however, controls whether or not a ZipX is automatically produced
	// where the best compression algorithm for each file is automatically chosen based
	// on file type. This property is for writing zip archives. It does not apply to
	// when unzipping ZIPX archives, Chilkat Zip automatically handles the various
	// compression algorithms when unzipping.
	// 
	// Important: Unfortunately, the ability to create ZIPX archives did not yet make
	// it into v9.4.1. This will come at a later date. Therefore, this property is
	// ignored for now.
	// 
	bool get_Zipx(void);
	// Starting in v9.4.1, Chilkat Zip will automatically unzip ZIPX files using any of
	// the following compression methods: BZIP2, PPMd, LZMA, and Deflate64 ("Deflate64"
	// is a trademark of PKWare, Inc.)
	// 
	// This property, however, controls whether or not a ZipX is automatically produced
	// where the best compression algorithm for each file is automatically chosen based
	// on file type. This property is for writing zip archives. It does not apply to
	// when unzipping ZIPX archives, Chilkat Zip automatically handles the various
	// compression algorithms when unzipping.
	// 
	// Important: Unfortunately, the ability to create ZIPX archives did not yet make
	// it into v9.4.1. This will come at a later date. Therefore, this property is
	// ignored for now.
	// 
	void put_Zipx(bool newVal);

	// The default compression algorithm to be used when creating ZIPX archives. The
	// default value is "deflate". Other possible values are "ppmd", "lzma", "bzip2"
	// and "deflate64". When writing a ZIPX archive, if the file extension does not
	// indicate an obvious choice for the appropriate compression algorithm, then the
	// ZipxDefaultAlg is used.
	// 
	// Important: Starting in v9.4.1, Chilkat Zip can automatically unzip ZIPX
	// archives, but the ability to create ZIPX archives did not make it into v9.4.1.
	// This feature will come at a later date. Currently, this property is ignored.
	// 
	void get_ZipxDefaultAlg(CkString &str);
	// The default compression algorithm to be used when creating ZIPX archives. The
	// default value is "deflate". Other possible values are "ppmd", "lzma", "bzip2"
	// and "deflate64". When writing a ZIPX archive, if the file extension does not
	// indicate an obvious choice for the appropriate compression algorithm, then the
	// ZipxDefaultAlg is used.
	// 
	// Important: Starting in v9.4.1, Chilkat Zip can automatically unzip ZIPX
	// archives, but the ability to create ZIPX archives did not make it into v9.4.1.
	// This feature will come at a later date. Currently, this property is ignored.
	// 
	const wchar_t *zipxDefaultAlg(void);
	// The default compression algorithm to be used when creating ZIPX archives. The
	// default value is "deflate". Other possible values are "ppmd", "lzma", "bzip2"
	// and "deflate64". When writing a ZIPX archive, if the file extension does not
	// indicate an obvious choice for the appropriate compression algorithm, then the
	// ZipxDefaultAlg is used.
	// 
	// Important: Starting in v9.4.1, Chilkat Zip can automatically unzip ZIPX
	// archives, but the ability to create ZIPX archives did not make it into v9.4.1.
	// This feature will come at a later date. Currently, this property is ignored.
	// 
	void put_ZipxDefaultAlg(const wchar_t *newVal);



	// ----------------------
	// Methods
	// ----------------------
#if defined(CK_SFX_INCLUDED)
	// (Relevant only when running on a Microsoft Windows operating system.) Embeds a
	// Zip file into an EXE as a custom resource. This resource can be opened by the
	// EXE containing it at runtime by using the OpenMyEmbedded method.
	// 
	// Important: In Visual Studio 2010, the linker has a property "Randomized Base
	// Address" (Project Properties/Linker) that defaults to "YES", but the default is
	// "NO" in Visual Studio 2008. (The property is nonexistent in Visual Studio 2005
	// and earlier.) This enables ASLR ( Address Space Layout Randomization) in Vista
	// and up, and prevents the proper code injection in the executable address space.
	// To successfully embed a .zip as a resource within an EXE, this Visual Studio
	// property must be set to "NO".
	// 
	bool AddEmbedded(const wchar_t *exeFilename, const wchar_t *resourceName, const wchar_t *zipFilename);
#endif

	// Attempting to compress already-compressed data is usually a waste of CPU cycles
	// with little or no benefit. In fact, it is possible that attempting to compress
	// already-compressed data results in a slightly increased size. The Zip file
	// format allows for files to be "stored" rather than compressed. This allows the
	// file data to be streamed directly into a .zip without compression.
	// 
	// An instance of the Zip object has an internal list of "no compress" extensions.
	// A filename with a "no compress" extension is "stored" rather than compressed.
	// Additional "no compress" extensions may be added by calling this method (once
	// per file extension). You should pass the file extension, such as ".xyz" in fileExtension.
	// 
	// "no compress" extensions may be removed by calling RemoveNoCompressExtension.
	// 
	// The default "no compress" extensions are: .zip, .gif, .jpg, .gz, .rar, .jar,
	// .tgz, .bz2, .z, .rpm, .msi, .png
	// 
	void AddNoCompressExtension(const wchar_t *fileExtension);

	// Creates a new Zip entry and initializes it with already-compressed data that is
	// Base64 encoded. (The ZipEntry.CopyBase64 method can be used to retrieve the
	// compressed data in Base64 format.)
	// 
	// Note 1: This method only updates the zip object. To update (rewrite) a zip file,
	// either the WriteZip or WriteZipAndClose method would need to be called.
	// 
	// Note 2: It is assumed that the compressed data is unencrypted deflated data.
	// (Meaning data compressed using the "deflate" compression algorithm.)
	// 
	// The caller is responsible for deleting the object returned by this method.
	CkZipEntryW *AppendBase64(const wchar_t *fileName, const wchar_t *encodedCompressedData);

	// Append memory data that is already Zip-compressed to the Zip object. The
	// ZipEntry object containing the compressed data is returned. Note: This method
	// appends the compressed data for a single zip entry. To load an entire in-memory
	// .zip, call OpenFromMemory instead.
	// 
	// Note 1: This method only updates the zip object. To update (rewrite) a zip file,
	// either the WriteZip or WriteZipAndClose method would need to be called.
	// 
	// Note 2: It is assumed that the compressed data is unencrypted deflated data.
	// (Meaning data compressed using the "deflate" compression algorithm.)
	// 
	// The caller is responsible for deleting the object returned by this method.
	CkZipEntryW *AppendCompressed(const wchar_t *fileName, const CkByteData &inData);

	// Appends in-memory data as a new entry to a Zip object. The ZipEntry object
	// containing the data is returned.
	// 
	// Note: This method only updates the zip object. To update (rewrite) a zip file,
	// either the WriteZip or WriteZipAndClose method would need to be called.
	// 
	// The caller is responsible for deleting the object returned by this method.
	CkZipEntryW *AppendData(const wchar_t *fileName, const CkByteData &inData);

	// Appends one or more files to the Zip object. The filePattern can use the "*"
	// wildcard character for 0 or more of any characterSet recurse equal to True to
	// recursively add all subdirectories, or False to only add files in the current
	// directory.
	// 
	// Note: This method only updates the zip object. To update (rewrite) a zip file,
	// either the WriteZip or WriteZipAndClose method would need to be called.
	// 
	bool AppendFiles(const wchar_t *filePattern, bool recurse);

	// Appends one or more files to the Zip object. The ARG1 can use the "*" and "?"
	// wildcard characters where "*" means 0 or more of any character and "?" means any
	// single character. The ARG2 controls whether directories are recursively
	// traversed. Set ARG2 equal to true to append files and subdirectories in the
	// directory tree. Set ARG2 equal to false to add files only from the indicated
	// directory.
	// 
	// The ARG3 only applies when the ARG1 is an absolute path pattern, such as
	// "C:/temp/abc/*.txt". If ARG3 is true, then the absolute path will be included
	// in the zip entry filenames as relative paths. For example, "temp/abc/xyz.txt".
	// 
	// The ARG4, ARG4, and ARG5 flags only apply when on the Windows operating system.
	// These flags control whether files with the Archive, Hidden, or System attributes
	// are included.
	// 
	// Note: This method does not write the zip archive. It simply adds references to
	// the files that will be included in the .zip when the WriteZip or
	// WriteZipAndClose methods are eventually called. Files and/or data may be added
	// to the zip object by calling any combination of the Append* methods before
	// finally writing the zip via one of the Write* methods.
	// 
	bool AppendFilesEx(const wchar_t *filePattern, bool recurse, bool saveExtraPath, bool archiveOnly, bool includeHidden, bool includeSystem);

	// Creates a new Zip entry and initializes it with already-compressed data that is
	// hexidecimal encoded. (The ZipEntry.CopyHex method can be used to retrieve the
	// compressed data in Hex format.)
	// 
	// Note 1: This method only updates the zip object. To update (rewrite) a zip file,
	// either the WriteZip or WriteZipAndClose method would need to be called.
	// 
	// Note 2: It is assumed that the compressed data is unencrypted deflated data.
	// (Meaning data compressed using the "deflate" compression algorithm.)
	// 
	// The caller is responsible for deleting the object returned by this method.
	CkZipEntryW *AppendHex(const wchar_t *fileName, const wchar_t *encodedCompressedData);

	// This method is the same as calling AppendFiles multiple times - once for each
	// file pattern in fileSpecs
	// 
	// Note: This method only updates the zip object. To update (rewrite) a zip file,
	// either the WriteZip or WriteZipAndClose method would need to be called.
	// 
	bool AppendMultiple(CkStringArrayW &fileSpecs, bool recurse);

	// Appends a new and empty entry to the Zip object and returns the ZipEntry object.
	// Data can be appended to the entry by calling ZipEntry.AppendData.
	// 
	// Note: This method only updates the zip object. To update (rewrite) a zip file,
	// either the WriteZip or WriteZipAndClose method would need to be called.
	// 
	// The caller is responsible for deleting the object returned by this method.
	CkZipEntryW *AppendNew(const wchar_t *fileName);

	// Adds an entry to the zip so that when it unzips, a new directory (with no files)
	// is created. The directory does not need to exist on the local filesystem when
	// calling this method. The dirName is simply a string that is used as the directory
	// path for the entry added to the zip. The zip entry object is returned.
	// 
	// Note: This method only updates the zip object. To update (rewrite) a zip file,
	// either the WriteZip or WriteZipAndClose method would need to be called.
	// 
	// The caller is responsible for deleting the object returned by this method.
	CkZipEntryW *AppendNewDir(const wchar_t *dirName);

	// Appends a single file or directory to the Zip object. The  saveExtraPath applies when fileOrDirPath
	// is an absolute (non-relative) path. If  saveExtraPath is true, then the absolute path is
	// made relative and saved in the zip. For example, if the fileOrDirPath is
	// "C:/temp/xyz/test.txt" and  saveExtraPath is true, then the path in the zip will be
	// "./temp/xyz/test.txt". If however, fileOrDirPath contains a relative path, then  saveExtraPath has
	// no effect.
	bool AppendOneFileOrDir(const wchar_t *fileOrDirName, bool saveExtraPath);

	// Adds an in-memory string to the Zip object. The  textData argument is converted to
	// the ANSI charset before being added to the Zip. If the Zip were written to disk
	// by calling WriteZip, and later unzipped, the entry would unzip to an ANSI text
	// file.
	// 
	// Note: This method only updates the zip object. To update (rewrite) a zip file,
	// either the WriteZip or WriteZipAndClose method would need to be called.
	// 
	// The caller is responsible for deleting the object returned by this method.
	CkZipEntryW *AppendString(const wchar_t *fileName, const wchar_t *str);

	// Same as AppendString, but allows the charset to be specified. The  textData is
	// converted to  charset before being added to the zip. The internalZipFilepath is the path of the
	// file that will be stored within the zip.
	// 
	// Note: This method only updates the zip object. To update (rewrite) a zip file,
	// either the WriteZip or WriteZipAndClose method would need to be called.
	// 
	// The caller is responsible for deleting the object returned by this method.
	CkZipEntryW *AppendString2(const wchar_t *fileName, const wchar_t *str, const wchar_t *charset);

	// Adds the contents of another existing Zip file to this Zip object.
	bool AppendZip(const wchar_t *zipFileName);

	// Closes an open Zip file. This is identical to calling NewZip. (NewZip closes the
	// current Zip file, if open, and initializes the Zip object to be empty. Zip files
	// are only created when WriteZip is called.)
	void CloseZip(void);

	// Removes a Zip entry from the calling Zip object.
	bool DeleteEntry(CkZipEntryW &entry);

	// Adds a directory name to be excluded when AppendFiles is called to add an entire
	// directory tree. All directories having a name equal to an excluded directory
	// will not be included when AppendFiles (or AppendFileEx) is called. Multiple
	// directories can be excluded by calling ExcludeDir multiple times.
	void ExcludeDir(const wchar_t *dirName);

	// Unzip all the files into the specified directory. Subdirectories are
	// automatically created as needed.
	bool Extract(const wchar_t *dirPath);

	// Unzips all the files in a Zip into a single directory regardless of the path
	// stored in the Zip
	bool ExtractInto(const wchar_t *dirPath);

	// Unzip all files matching a wildcard pattern.
	bool ExtractMatching(const wchar_t *dirPath, const wchar_t *pattern);

	// Extracts only the files that have more recent last-modified-times than the files
	// on disk. This allows you to easily refresh only the files that have been
	// updated.
	bool ExtractNewer(const wchar_t *dirPath);

	// Identical to calling ZipEntry.Extract, but allows for progress monitoring
	// because the ZipEntry object does not have progress monitoring (i.e. event
	// callback) capabilities.
	bool ExtractOne(CkZipEntryW &entry, const wchar_t *dirPath);

	// Return the first entry in the Zip. Call ZipEntry.NextEntry to iterate over the
	// entries in a Zip until a NULL is returned.
	// The caller is responsible for deleting the object returned by this method.
	CkZipEntryW *FirstEntry(void);

	// Returns the first entry having a filename matching a pattern. The "*" characters
	// matches 0 or more of any character. The full filename, including path, is used
	// when matching against the pattern. A NULL is returned if nothing matches.
	// The caller is responsible for deleting the object returned by this method.
	CkZipEntryW *FirstMatchingEntry(const wchar_t *pattern);

	// Return the contents of the Zip file directory in an XML formatted string
	bool GetDirectoryAsXML(CkString &outXml);
	// Return the contents of the Zip file directory in an XML formatted string
	const wchar_t *getDirectoryAsXML(void);
	// Return the contents of the Zip file directory in an XML formatted string
	const wchar_t *directoryAsXML(void);

	// Retrieves a ZipEntry by ID. Chilkat Zip.NET automatically assigns a unique ID to
	// each ZipEntry in the Zip. This feature makes it easy to associate an item in a
	// UI control with a ZipEntry.
	// The caller is responsible for deleting the object returned by this method.
	CkZipEntryW *GetEntryByID(int entryID);

	// Retrieves a ZipEntry by index. The first entry is at index 0. This will return
	// directory entries as well as files.
	// The caller is responsible for deleting the object returned by this method.
	CkZipEntryW *GetEntryByIndex(int index);

	// Returns a ZipEntry by filename. If a full or partial path is part of the
	// filename, this must be included in the filename parameter.
	// The caller is responsible for deleting the object returned by this method.
	CkZipEntryW *GetEntryByName(const wchar_t *entryName);

	// Returns the current collection of exclusion patterns that have been set by
	// SetExclusions.
	// The caller is responsible for deleting the object returned by this method.
	CkStringArrayW *GetExclusions(void);

#if defined(CK_SFX_INCLUDED)
	// (Relevant only when running on a Microsoft Windows operating system.) Gets the
	// value of an EXE config param as described in the ExeXmlConfig property.
	bool GetExeConfigParam(const wchar_t *name, CkString &outStr);
	// (Relevant only when running on a Microsoft Windows operating system.) Gets the
	// value of an EXE config param as described in the ExeXmlConfig property.
	const wchar_t *getExeConfigParam(const wchar_t *name);
	// (Relevant only when running on a Microsoft Windows operating system.) Gets the
	// value of an EXE config param as described in the ExeXmlConfig property.
	const wchar_t *exeConfigParam(const wchar_t *name);
#endif

	// Inserts a new and empty entry into the Zip object. To insert at the beginning of
	// the Zip, beforeIndex should be 0. The ZipEntry's FileName property is
	// initialized to fileName parameter.
	// The caller is responsible for deleting the object returned by this method.
	CkZipEntryW *InsertNew(const wchar_t *fileName, int beforeIndex);

	// Returns true if the fileExtension is contained in the set of "no compress" extensions,
	// otherwise returns false. (See the documentation for the AddNoCompressExtension
	// method.) The fileExtension may be passed with or without the ".". For example, both
	// ".jpg" and "jpg" are acceptable.
	bool IsNoCompressExtension(const wchar_t *fileExtension);

	// Return True if a Zip file is password protected
	bool IsPasswordProtected(const wchar_t *zipFilename);

	// Returns True if the class is already unlocked, otherwise returns False.
	bool IsUnlocked(void);

	// Clears and initializes the contents of the Zip object. If a Zip file was open,
	// it is closed and all entries are removed from the object. The FileName property
	// is set to the zipFilePath argument.
	bool NewZip(const wchar_t *ZipFileName);

#if defined(CK_SFX_INCLUDED)
	// (Relevant only when running on a Microsoft Windows operating system.) Opens a
	// Zip embedded in an MS Windows EXE
	bool OpenEmbedded(const wchar_t *exeFilename, const wchar_t *resourceName);
#endif

	// Same as OpenFromMemory.
	bool OpenFromByteData(const CkByteData &byteData);

	// Open a Zip that is completely in-memory. This allows for Zip files to be opened
	// from non-filesystem sources, such as a database.
	bool OpenFromMemory(const CkByteData &inData);

#if defined(CK_SFX_INCLUDED)
	// (Relevant only when running on a Microsoft Windows operating system.) Opens a
	// Zip embedded within the caller's MS Windows EXE.
	bool OpenMyEmbedded(const wchar_t *resourceName);
#endif

	// Opens a Zip archive. Encrypted and password-protected zips may be opened without
	// providing the password, but their contents may not be unzipped unless the
	// correct password is provided via the DecryptPassword proprety, or the
	// SetPassword method.
	// 
	// When a zip is opened, the PasswordProtect and Encryption properties will be
	// appropriately set. If the zip is password protected (i.e. uses older Zip 2.0
	// encrypion), then the PasswordProtect property will be set to true. If the zip
	// is strong encrypted, the Encryption property will be set to a value 1 through 4,
	// where 4 indicates WinZip compatible AES encryption.
	// 
	bool OpenZip(const wchar_t *ZipFileName);

	// Efficiently appends additional files to an existing zip archive. QuickAppend
	// leaves all entries in the existing .zip untouched. It operates by appending new
	// files and updating the internal "central directory" of the zip archive.
	bool QuickAppend(const wchar_t *ZipFileName);

#if defined(CK_SFX_INCLUDED)
	// (Relevant only when running on a Microsoft Windows operating system.) Removes an
	// embedded Zip from an MS-Windows EXE
	bool RemoveEmbedded(const wchar_t *exeFilename, const wchar_t *resourceName);
#endif

	// Removes a file extension from the zip object's internal list of "no compress"
	// extensions. (For more information, see AddNoCompressExtension.)
	void RemoveNoCompressExtension(const wchar_t *fileExtension);

#if defined(CK_SFX_INCLUDED)
	// (Relevant only when running on a Microsoft Windows operating system.) Replace a
	// Zip embedded in an MS-Windows EXE with another Zip file.
	bool ReplaceEmbedded(const wchar_t *exeFilename, const wchar_t *resourceName, const wchar_t *zipFilename);
#endif

	// Sets the compression level for all Zip entries. The default compression level is
	// 6. A compression level of 0 is equivalent to no compression. The maximum
	// compression level is 9.
	// 
	// The zip.SetCompressionLevel method must be called after appending the files
	// (i.e. after the calls to AppendFile*, AppendData, or AppendOneFileOrDir).
	// 
	// A single call to SetCompressionLevel will set the compression level for all
	// existing entries.
	// 
	void SetCompressionLevel(int level);

	// Specify a collection of exclusion patterns to be used when adding files to a
	// Zip. Each pattern in the collection can use the "*" wildcard character, where
	// "*" indicates 0 or more occurances of any character.
	void SetExclusions(const CkStringArrayW &excludePatterns);

#if defined(CK_SFX_INCLUDED)
	// Sets a self-extractor property that is embedded in the resultant EXE created by
	// the WriteExe or WriteExe2 methods. The paramName is one of the XML tags listed in the
	// ExeXmlConfig property.
	// 
	// For example, to specify the text for the self-extractor's main dialog unzip
	// button, paramName would be "MainUnzipBtn".
	// 
	void SetExeConfigParam(const wchar_t *name, const wchar_t *value);
#endif

	// Set the password for an encrypted or password-protected Zip.
	void SetPassword(const wchar_t *password);

	// Unlocks the component allowing for the full functionality to be used. If a
	// permanent (purchased) unlock code is passed, there is no expiration. Any other
	// string automatically begins a fully-functional 30-day trial the first time
	// UnlockComponent is called.
	bool UnlockComponent(const wchar_t *regCode);

	// Unzips and returns the number of files unzipped, or -1 if a failure occurs.
	// Subdirectories are automatically created during the unzipping process.
	int Unzip(const wchar_t *dirPath);

	// Unzips and returns the number of files unzipped, or -1 if a failure occurs. All
	// files in the Zip are unzipped into the specfied dirPath regardless of the
	// directory path information contained in the Zip. This has the effect of
	// collapsing all files into a single directory. If several files in the Zip have
	// the same name, the files unzipped last will overwrite the files already
	// unzipped.
	int UnzipInto(const wchar_t *dirPath);

	// Same as Unzip, but only unzips files matching a pattern. If no wildcard
	// characters ('*') are used, then only files that exactly match the pattern will
	// be unzipped. The "*" characters matches 0 or more of any character.
	int UnzipMatching(const wchar_t *dirPath, const wchar_t *pattern, bool verbose);

	// Unzips matching files into a single directory, ignoring all path information
	// stored in the Zip.
	int UnzipMatchingInto(const wchar_t *dirPath, const wchar_t *pattern, bool verbose);

	// Same as Unzip, but only files that don't already exist on disk, or have later
	// file modification dates are unzipped.
	int UnzipNewer(const wchar_t *dirPath);

	// Tests the current DecryptPassword setting against the currently opened zip.
	// Returns true if the password is valid, otherwise returns false.
	bool VerifyPassword(void);

#if defined(CK_SFX_INCLUDED)
	// (Relevant only when running on a Microsoft Windows operating system.) Writes an
	// MS-Windows self-extracting executable. There are no limitations on the total
	// size, individual file size, or number of files that can be added to a
	// self-extracting EXE.
	// 
	// If the resultant EXE will automatically accept these command-line arguments when
	// run:
	// 
	// -log logFileName
	// 
	// Creates a log file that lists the settings embedded within the EXE and logs the
	// errors, warnings, and other information about the self-extraction.
	// 
	// -unzipDir unzipDirectoryPath
	// 
	// Unzips to this directory path without user intervention.
	// 
	// -pwd password
	// 
	// Specifies the password for an encrypted EXE
	// 
	// -ap autoRunParams
	// 
	// Specifies the command line parameters to be passed to the AutoRun executable
	// (embedded within the EXE).
	// 
	bool WriteExe(const wchar_t *exeFilename);
#endif

#if defined(CK_SFX_INCLUDED)
	// (Relevant only when running on a Microsoft Windows operating system.) Writes a
	// self-extracting MS-Windows EXE with no limitations on total file size and no
	// limitations on the size of any one file contained within. The 1st argument is
	// the pre-existing EXE housing that is to be used. Essentially, the
	// self-extracting EXE is a concatenation of the EXE housing and the
	// compressed/encrypted data. The 2nd argument is the name of the EXE to create or
	// overwrite. A housing for use with WriteExe2 can be found here:
	// http://www.chilkatsoft.com/d2/SaExtract.zip
	bool WriteExe2(const wchar_t *exePath, const wchar_t *destExePath, bool bAesEncrypt, int keyLength, const wchar_t *password);
#endif

#if defined(CK_SFX_INCLUDED)
	// (Relevant only when running on a Microsoft Windows operating system.) Same as
	// WriteExe, but instead of writing a file, the MS-Windows EXE is written to
	// memory.
	bool WriteExeToMemory(CkByteData &outBytes);
#endif

	// Same as WriteZip, but instead of writing the Zip to a file, it writes to memory.
	// Zips that are written to memory can also be opened from memory by calling
	// OpenFromMemory.
	bool WriteToMemory(CkByteData &outData);

	// Saves the Zip to a file and implictly re-opens it so further operations can
	// continue. Use WriteZipAndClose to write and close the Zip. There is no
	// limitation on the size of files that may be contained within a .zip, the total
	// number of files in a .zip, or the total size of a .zip. If necessary, WriteZip
	// will use the ZIP64 file format extensions when 4GB or file count limitations of
	// the old zip file format are exceeded.
	bool WriteZip(void);

	// Saves the Zip to a file and closes it. On return, the Zip object will be in the
	// state as if NewZip had been called. There is no limitation on the size of files
	// that may be contained within a .zip, the total number of files in a .zip, or the
	// total size of a .zip. If necessary, WriteZip will use the ZIP64 file format
	// extensions when 4GB or file count limitations of the old zip file format are
	// exceeded.
	bool WriteZipAndClose(void);





	// END PUBLIC INTERFACE


};
#ifndef __sun__
#pragma pack (pop)
#endif


	
#endif
