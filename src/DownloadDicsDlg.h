#ifndef DOWNLOAD_DICS_DLG_H
#define DOWNLOAD_DICS_DLG_H

#include "staticdialog\staticdialog.h"
enum FTP_OPERATION_TYPE
{
  FILL_FILE_LIST = 0,
  DOWNLOAD_FILE,
};
class SpellChecker;

class DownloadDicsDlg : public StaticDialog
{
public:
  void DoDialog ();
  // Maybe hunspell interface should be passed here
  void init (HINSTANCE hInst, HWND Parent, SpellChecker *SpellCheckerInstanceArg);
  BOOL CALLBACK run_dlgProc (UINT message, WPARAM wParam, LPARAM lParam);
  void DoFtpOperation (FTP_OPERATION_TYPE Type, TCHAR *Address, TCHAR *FileName = 0, TCHAR *Location = 0);
  void DownloadSelected ();
private:
  void OnDisplayAction ();
private:
  SpellChecker *SpellCheckerInstance;
  HWND HFileList;
  HWND HAddress;
  HWND HStatus;
};
#endif // DOWNLOAD_DICS_DLG_H