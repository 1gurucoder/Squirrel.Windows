#include "stdafx.h"
#include "unzip.h"
#include "Resource.h"
#include "UpdateRunner.h"

void CUpdateRunner::DisplayErrorMessage(CString& errorMessage)
{
	CTaskDialog dlg;

	// TODO: Something about contacting support?
	dlg.SetCommonButtons(TDCBF_OK_BUTTON);
	dlg.SetMainInstructionText(L"Installation has failed");
	dlg.SetContentText(errorMessage);
	dlg.SetMainIcon(TD_ERROR_ICON);

	dlg.DoModal();
}

int CUpdateRunner::ExtractUpdaterAndRun(wchar_t* lpCommandLine)
{
	wchar_t targetDir[MAX_PATH];

	ExpandEnvironmentStrings(L"%LocalAppData%\SquirrelTemp", targetDir, MAX_PATH);
	if (GetFileAttributes(targetDir) == INVALID_FILE_ATTRIBUTES) {
		if (!CreateDirectory(targetDir, NULL)) {
			goto failedExtract;
		}
	}

	CResource zipResource;
	if (!zipResource.Load(L"DATA", IDR_UPDATE_ZIP)) {
		goto failedExtract;
	}

	DWORD dwSize = zipResource.GetSize();
	if (dwSize < 0x100) {
		goto failedExtract;
	}

	BYTE* pData = (BYTE*)zipResource.Lock();
	HZIP zipFile = OpenZip(pData, dwSize, NULL);
	SetUnzipBaseDir(zipFile, targetDir);

	// NB: This library is kind of a disaster
	ZRESULT zr;
	ZIPENTRY zentry;
	int index = 0;
	do {
		zr = GetZipItem(zipFile, index++, &zentry);
		if (zr != ZR_OK && zr != ZR_MORE) {
			break;
		}
		zr = UnzipItem(zipFile, index, zentry.name);
	} while (zr == ZR_MORE);

	CloseZip(zipFile);
	zipResource.Release();

	// nfi if the zip extract actually worked, check for Update.exe
	wchar_t updateExePath[MAX_PATH];
	swprintf_s(updateExePath, sizeof(wchar_t)*MAX_PATH, L"%s\\%s", targetDir, L"Update.exe");

	if (GetFileAttributes(updateExePath) == INVALID_FILE_ATTRIBUTES) {
		goto failedExtract;
	}

	// Run Update.exe
	PROCESS_INFORMATION pi = { 0 };
	STARTUPINFO si = { 0 };
	si.cb = sizeof(STARTUPINFO);
	si.wShowWindow = SW_SHOW;
	si.dwFlags = STARTF_USESHOWWINDOW;

	if (!CreateProcess(updateExePath, lpCommandLine, NULL, NULL, false, 0, NULL, NULL, &si, &pi)) {
		goto failedExtract;
	}

	WaitForSingleObject(pi.hProcess, INFINITE);

	DWORD dwExitCode;
	if (!GetExitCodeProcess(pi.hProcess, &dwExitCode)) {
		dwExitCode = (DWORD)-1;
	}

	CloseHandle(pi.hProcess);
	CloseHandle(pi.hThread);
	return (int) dwExitCode;

failedExtract:
	DisplayErrorMessage(CString(L"Failed to extract installer"));
}