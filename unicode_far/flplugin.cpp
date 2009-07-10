/*
flplugin.cpp

�������� ������ - ������ � ���������
*/
/*
Copyright (c) 1996 Eugene Roshal
Copyright (c) 2000 Far Group
All rights reserved.

Redistribution and use in source and binary forms, with or without
modification, are permitted provided that the following conditions
are met:
1. Redistributions of source code must retain the above copyright
   notice, this list of conditions and the following disclaimer.
2. Redistributions in binary form must reproduce the above copyright
   notice, this list of conditions and the following disclaimer in the
   documentation and/or other materials provided with the distribution.
3. The name of the authors may not be used to endorse or promote products
   derived from this software without specific prior written permission.

THIS SOFTWARE IS PROVIDED BY THE AUTHOR ``AS IS'' AND ANY EXPRESS OR
IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES
OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED.
IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR ANY DIRECT, INDIRECT,
INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT
NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
(INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF
THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
*/

#include "headers.hpp"
#pragma hdrstop

#include "lang.hpp"
#include "filelist.hpp"
#include "filepanels.hpp"
#include "history.hpp"
#include "ctrlobj.hpp"
#include "syslog.hpp"
#include "message.hpp"
#include "config.hpp"
#include "delete.hpp"
#include "datetime.hpp"
#include "dirmix.hpp"
#include "pathmix.hpp"
#include "mix.hpp"

/*
   � ����� ������ ������ �� �������� - ������ ����������!
*/

void FileList::PushPlugin(HANDLE hPlugin,const wchar_t *HostFile)
{
  PluginsStackItem *stItem = new PluginsStackItem;
  stItem->hPlugin=hPlugin;
  stItem->strHostFile = HostFile; //??NULL??
  stItem->Modified=FALSE;
  stItem->PrevViewMode=ViewMode;
  stItem->PrevSortMode=SortMode;
  stItem->PrevSortOrder=SortOrder;
  stItem->PrevNumericSort=NumericSort;
	memmove(&stItem->PrevViewSettings,&ViewSettings,sizeof(PanelViewSettings));

  PluginsStack=(PluginsStackItem **)xf_realloc(PluginsStack,(PluginsStackSize+1)*sizeof(*PluginsStack));
  PluginsStack[PluginsStackSize] = stItem;
  PluginsStackSize++;
}


int FileList::PopPlugin(int EnableRestoreViewMode)
{
	OpenPluginInfo Info={0};

  if (PluginsStackSize==0)
  {
    PanelMode=NORMAL_PANEL;
    return(FALSE);
  }

  PluginsStackSize--;

  // ��������� ������� ������.
  CtrlObject->Plugins.ClosePlugin(hPlugin);

  PluginsStackItem *PStack=PluginsStack[PluginsStackSize]; // ��������� �� ������, � �������� ������

  if (PluginsStackSize>0)
  {
    hPlugin=PluginsStack[PluginsStackSize-1]->hPlugin;

    if (EnableRestoreViewMode)
    {
      SetViewMode (PStack->PrevViewMode);

      SortMode=PStack->PrevSortMode;
      NumericSort=PStack->PrevNumericSort;
      SortOrder=PStack->PrevSortOrder;
    }

    if (PStack->Modified)
    {
			PluginPanelItem PanelItem;

      string strSaveDir;

			apiGetCurrentDirectory(strSaveDir);

      if (FileNameToPluginItem(PStack->strHostFile,&PanelItem))
        CtrlObject->Plugins.PutFiles(hPlugin,&PanelItem,1,FALSE,0);
      else
      {
        memset(&PanelItem,0,sizeof(PanelItem));

        PanelItem.FindData.lpwszFileName = xf_wcsdup(PointToName(PStack->strHostFile));
        CtrlObject->Plugins.DeleteFiles(hPlugin,&PanelItem,1,0);

        xf_free (PanelItem.FindData.lpwszFileName);
      }

      FarChDir(strSaveDir);
    }

    CtrlObject->Plugins.GetOpenPluginInfo(hPlugin,&Info);

    if ((Info.Flags & OPIF_REALNAMES)==0)
    {
      DeleteFileWithFolder(PStack->strHostFile);  // �������� ����� �� ����������� �������
    }
  }
  else
  {
    PanelMode=NORMAL_PANEL;
    if(EnableRestoreViewMode)
    {
      SetViewMode (PStack->PrevViewMode);

      SortMode=PStack->PrevSortMode;
      NumericSort=PStack->PrevNumericSort;
      SortOrder=PStack->PrevSortOrder;
    }
  }

  delete PluginsStack[PluginsStackSize];

  if(PluginsStackSize)
		PluginsStack=(PluginsStackItem **)xf_realloc(PluginsStack,PluginsStackSize*sizeof(*PluginsStack));
	else
	{
		xf_free(PluginsStack);
		PluginsStack=NULL;
	}

  if (EnableRestoreViewMode)
    CtrlObject->Cp()->RedrawKeyBar();

  return(TRUE);
}


int FileList::FileNameToPluginItem(const wchar_t *Name,PluginPanelItem *pi)
{
	string strTempDir = Name;

	if (!CutToSlash(strTempDir,true))
		return(FALSE);

	FarChDir(strTempDir);
	memset(pi,0,sizeof(*pi));

	FAR_FIND_DATA_EX fdata;

	if ( apiGetFindDataEx (Name, &fdata) )
	{
		apiFindDataExToData(&fdata, &pi->FindData);
		return TRUE;
	}

	return FALSE;
}


void FileList::FileListToPluginItem(FileListItem *fi,PluginPanelItem *pi)
{
  pi->FindData.lpwszFileName = xf_wcsdup (fi->strName);
  pi->FindData.lpwszAlternateFileName = xf_wcsdup (fi->strShortName);

  pi->FindData.nFileSize=fi->UnpSize;
  pi->FindData.nPackSize=fi->PackSize;
  pi->FindData.dwFileAttributes=fi->FileAttr;
  pi->FindData.ftLastWriteTime=fi->WriteTime;
  pi->FindData.ftCreationTime=fi->CreationTime;
  pi->FindData.ftLastAccessTime=fi->AccessTime;
  pi->NumberOfLinks=fi->NumberOfLinks;
  pi->Flags=fi->UserFlags;
  if (fi->Selected)
    pi->Flags|=PPIF_SELECTED;
  pi->CustomColumnData=fi->CustomColumnData;
  pi->CustomColumnNumber=fi->CustomColumnNumber;

  pi->Description=fi->DizText; //BUGBUG???

  if (fi->UserData && (fi->UserFlags & PPIF_USERDATA))
  {
    DWORD Size=*(DWORD *)fi->UserData;
    pi->UserData=(DWORD_PTR)xf_malloc(Size);
    memcpy((void *)pi->UserData,(void *)fi->UserData,Size);
  }
  else
    pi->UserData=fi->UserData;
  pi->CRC32=fi->CRC32;
  pi->Reserved[0]=pi->Reserved[1]=0;
  pi->Owner=fi->strOwner.IsEmpty()?NULL:(wchar_t*)(const wchar_t*)fi->strOwner;
}

void FileList::FreePluginPanelItem(PluginPanelItem *pi)
{
	apiFreeFindData(&pi->FindData);
	if(pi->UserData && (pi->Flags & PPIF_USERDATA))
			xf_free((void*)pi->UserData);
}

size_t FileList::FileListToPluginItem2(FileListItem *fi,PluginPanelItem *pi)
{
	size_t size=sizeof(*pi);
	size+=sizeof(wchar_t)*(fi->strName.GetLength()+1);
	size+=sizeof(wchar_t)*(fi->strShortName.GetLength()+1);
	size+=fi->strOwner.IsEmpty()?0:sizeof(wchar_t)*(fi->strOwner.GetLength()+1);
	size+=(fi->DizText==NULL)?0:sizeof(wchar_t)*(wcslen(fi->DizText)+1);
	size+=fi->CustomColumnNumber*sizeof(wchar_t*);
	for(int ii=0;ii<fi->CustomColumnNumber;ii++)
	{
		size+=(fi->CustomColumnData[ii]==NULL)?0:sizeof(wchar_t)*(wcslen(fi->CustomColumnData[ii])+1);
	}
	if (fi->UserData && (fi->UserFlags & PPIF_USERDATA))
	{
		size+=*(DWORD *)fi->UserData;
	}
	if(pi)
	{
		char* data=(char*)(pi+1);
		pi->FindData.lpwszFileName=wcscpy((wchar_t*)data,fi->strName);
		data+=sizeof(wchar_t)*(fi->strName.GetLength()+1);
		pi->FindData.lpwszAlternateFileName=wcscpy((wchar_t*)data,fi->strShortName);
		data+=sizeof(wchar_t)*(fi->strShortName.GetLength()+1);
		pi->FindData.nFileSize=fi->UnpSize;
		pi->FindData.nPackSize=fi->PackSize;
		pi->FindData.dwFileAttributes=fi->FileAttr;
		pi->FindData.ftLastWriteTime=fi->WriteTime;
		pi->FindData.ftCreationTime=fi->CreationTime;
		pi->FindData.ftLastAccessTime=fi->AccessTime;
		pi->NumberOfLinks=fi->NumberOfLinks;
		pi->Flags=fi->UserFlags;
		if(fi->Selected) pi->Flags|=PPIF_SELECTED;
		pi->CustomColumnNumber=fi->CustomColumnNumber;
		pi->CustomColumnData=(wchar_t**)data;
		data+=fi->CustomColumnNumber*sizeof(wchar_t*);
		for(int ii=0;ii<fi->CustomColumnNumber;ii++)
		{
			if(fi->CustomColumnData[ii]==NULL)
			{
				pi->CustomColumnData[ii]=NULL;
			}
			else
			{
				pi->CustomColumnData[ii]=wcscpy((wchar_t*)data,fi->CustomColumnData[ii]);
				data+=sizeof(wchar_t)*(wcslen(fi->CustomColumnData[ii])+1);
			}
		}
		if(fi->DizText==NULL)
		{
			pi->Description=NULL;
		}
		else
		{
			pi->Description=wcscpy((wchar_t*)data,fi->DizText);
			data+=sizeof(wchar_t)*(wcslen(fi->DizText)+1);
		}

		if(fi->UserData&&(fi->UserFlags&PPIF_USERDATA))
		{
			DWORD Size=*(DWORD *)fi->UserData;
			pi->UserData=(DWORD_PTR)data;
			memcpy((void *)pi->UserData,(void *)fi->UserData,Size);
			data+=Size;
		}
		else
			pi->UserData=fi->UserData;
		pi->CRC32=fi->CRC32;
		pi->Reserved[0]=pi->Reserved[1]=0;
		if(fi->strOwner.IsEmpty())
		{
			pi->Owner=NULL;
		}
		else
		{
			pi->Owner=wcscpy((wchar_t*)data,fi->strOwner);
		}
	}
	return size;
}

void FileList::PluginToFileListItem(PluginPanelItem *pi,FileListItem *fi)
{
  fi->strName = pi->FindData.lpwszFileName;
  fi->strShortName = pi->FindData.lpwszAlternateFileName;

  fi->strOwner = pi->Owner;
  if (pi->Description)
  {
    fi->DizText=new wchar_t[StrLength(pi->Description)+1];

    wcscpy (fi->DizText, pi->Description);

    fi->DeleteDiz=TRUE;
  }
  else
    fi->DizText=NULL;
  fi->UnpSize=pi->FindData.nFileSize;
  fi->PackSize=pi->FindData.nPackSize;
  fi->FileAttr=pi->FindData.dwFileAttributes;
  fi->WriteTime=pi->FindData.ftLastWriteTime;
  fi->CreationTime=pi->FindData.ftCreationTime;
  fi->AccessTime=pi->FindData.ftLastAccessTime;
  fi->NumberOfLinks=pi->NumberOfLinks;
	fi->NumberOfStreams=1;
  fi->UserFlags=pi->Flags;

  if (pi->UserData && (pi->Flags & PPIF_USERDATA))
  {
    DWORD Size=*(DWORD *)pi->UserData;
    fi->UserData=(DWORD_PTR)xf_malloc(Size);
    memcpy((void *)fi->UserData,(void *)pi->UserData,Size);
  }
  else
    fi->UserData=pi->UserData;
  if (pi->CustomColumnNumber>0)
  {
    fi->CustomColumnData=new wchar_t*[pi->CustomColumnNumber];
    for (int I=0;I<pi->CustomColumnNumber;I++)
      if (pi->CustomColumnData!=NULL && pi->CustomColumnData[I]!=NULL)
      {
        fi->CustomColumnData[I]=new wchar_t[StrLength(pi->CustomColumnData[I])+1];
        wcscpy(fi->CustomColumnData[I],pi->CustomColumnData[I]);
      }
      else
      {
        fi->CustomColumnData[I]=new wchar_t[1];
        fi->CustomColumnData[I][0]=0;
      }
  }
  fi->CustomColumnNumber=pi->CustomColumnNumber;
  fi->CRC32=pi->CRC32;
}


HANDLE FileList::OpenPluginForFile(const wchar_t *FileName,DWORD FileAttr)
{
  if(!FileName || !*FileName || (FileAttr&FILE_ATTRIBUTE_DIRECTORY))
    return(INVALID_HANDLE_VALUE);

  SetCurPath();

	HANDLE hFile=apiCreateFile(FileName,GENERIC_READ,FILE_SHARE_READ|FILE_SHARE_WRITE ,NULL,OPEN_EXISTING,FILE_FLAG_SEQUENTIAL_SCAN);

  if (hFile==INVALID_HANDLE_VALUE)
  {
    //Message(MSG_WARNING|MSG_ERRORTYPE,1,MSG(MEditTitle),MSG(MCannotOpenFile),FileName,MSG(MOk));
    Message(MSG_WARNING|MSG_ERRORTYPE,1,L"",MSG(MOpenPluginCannotOpenFile),FileName,MSG(MOk));
    return(INVALID_HANDLE_VALUE);
  }

  char *Buffer=new char[Opt.PluginMaxReadData];
  if(Buffer)
  {
    DWORD BytesRead;
    _ALGO(SysLog(L"Read %d byte(s)",Opt.PluginMaxReadData));
    if(ReadFile(hFile,Buffer,Opt.PluginMaxReadData,&BytesRead,NULL))
    {
      CloseHandle(hFile);
      _ALGO(SysLogDump("First 128 bytes",0,(LPBYTE)Buffer,128,NULL));

      _ALGO(SysLog(L"close AnotherPanel file"));
      CtrlObject->Cp()->GetAnotherPanel(this)->CloseFile();

      _ALGO(SysLog(L"call Plugins.OpenFilePlugin {"));
      HANDLE hNewPlugin=CtrlObject->Plugins.OpenFilePlugin(FileName,(unsigned char *)Buffer,BytesRead,0);
      _ALGO(SysLog(L"}"));

      delete[] Buffer;

      return(hNewPlugin);
    }
    else
    {
      delete[] Buffer;
      _ALGO(SysLogLastError());
    }
  }

  CloseHandle(hFile);
  return(INVALID_HANDLE_VALUE);
}


void FileList::CreatePluginItemList(PluginPanelItem *(&ItemList),int &ItemNumber,BOOL AddTwoDot)
{
  if (!ListData)
    return;

  long SaveSelPosition=GetSelPosition;
  long OldLastSelPosition=LastSelPosition;

  string strSelName;
  DWORD FileAttr;
  ItemNumber=0;
  ItemList=new PluginPanelItem[SelFileCount+1];
  if (ItemList!=NULL)
  {
		memset(ItemList,0,sizeof(PluginPanelItem) * (SelFileCount+1));
    GetSelName(NULL,FileAttr);
    while (GetSelName(&strSelName,FileAttr))
      if (((FileAttr & FILE_ATTRIBUTE_DIRECTORY)==0 || !TestParentFolderName(strSelName))
          && LastSelPosition>=0 && LastSelPosition<FileCount)
      {
        FileListToPluginItem(ListData[LastSelPosition],ItemList+ItemNumber);
        ItemNumber++;
      }

    if(AddTwoDot && !ItemNumber && (FileAttr & FILE_ATTRIBUTE_DIRECTORY)) // ��� ��� ".."
    {
      FileListToPluginItem(ListData[0],ItemList+ItemNumber);
      //ItemList->FindData.lpwszFileName = xf_wcsdup (ListData[0]->strName);
      //ItemList->FindData.dwFileAttributes=ListData[0]->FileAttr;
      ItemNumber++;
    }
  }

  LastSelPosition=OldLastSelPosition;
  GetSelPosition=SaveSelPosition;
}


void FileList::DeletePluginItemList(PluginPanelItem *(&ItemList),int &ItemNumber)
{
	PluginPanelItem *PItemList=ItemList;
  if(PItemList)
  {
    for (int I=0;I<ItemNumber;I++,PItemList++)
    {
      FreePluginPanelItem(PItemList);
    }
    delete[] ItemList;
  }
}


void FileList::PluginDelete()
{
  _ALGO(CleverSysLog clv(L"FileList::PluginDelete()"));
	PluginPanelItem *ItemList;
  int ItemNumber;
  SaveSelection();
  CreatePluginItemList(ItemList,ItemNumber);
  if (ItemList!=NULL && ItemNumber>0)
  {
    if (CtrlObject->Plugins.DeleteFiles(hPlugin,ItemList,ItemNumber,0))
    {
      SetPluginModified();
      PutDizToPlugin(this,ItemList,ItemNumber,TRUE,FALSE,NULL,&Diz);
    }
    DeletePluginItemList(ItemList,ItemNumber);
    Update(UPDATE_KEEP_SELECTION);
    Redraw();
    Panel *AnotherPanel=CtrlObject->Cp()->GetAnotherPanel(this);
    AnotherPanel->Update(UPDATE_KEEP_SELECTION|UPDATE_SECONDARY);
    AnotherPanel->Redraw();
  }
}


void FileList::PutDizToPlugin(FileList *DestPanel,PluginPanelItem *ItemList,
                              int ItemNumber,int Delete,int Move,DizList *SrcDiz,
                              DizList *DestDiz)
{
  _ALGO(CleverSysLog clv(L"FileList::PutDizToPlugin()"));
	OpenPluginInfo Info;
  CtrlObject->Plugins.GetOpenPluginInfo(DestPanel->hPlugin,&Info);
  if ( DestPanel->strPluginDizName.IsEmpty() && Info.DescrFilesNumber>0)
    DestPanel->strPluginDizName = Info.DescrFiles[0];
  if (((Opt.Diz.UpdateMode==DIZ_UPDATE_IF_DISPLAYED && IsDizDisplayed()) ||
      Opt.Diz.UpdateMode==DIZ_UPDATE_ALWAYS) && !DestPanel->strPluginDizName.IsEmpty() &&
      (Info.HostFile==NULL || *Info.HostFile==0 || DestPanel->GetModalMode() ||
			apiGetFileAttributes(Info.HostFile)!=INVALID_FILE_ATTRIBUTES))
  {
    CtrlObject->Cp()->LeftPanel->ReadDiz();
    CtrlObject->Cp()->RightPanel->ReadDiz();

    if (DestPanel->GetModalMode())
      DestPanel->ReadDiz();

    int DizPresent=FALSE;
    for (int I=0;I<ItemNumber;I++)
      if (ItemList[I].Flags & PPIF_PROCESSDESCR)
      {
          string strName = ItemList[I].FindData.lpwszFileName;
          string strShortName = ItemList[I].FindData.lpwszAlternateFileName;

        int Code;
        if (Delete)
          Code=DestDiz->DeleteDiz(strName,strShortName);
        else
        {
          Code=SrcDiz->CopyDiz(strName,strShortName,strName,strShortName,DestDiz);
          if (Code && Move)
            SrcDiz->DeleteDiz(strName,strShortName);
        }
        if (Code)
          DizPresent=TRUE;
      }
    if (DizPresent)
    {
      string strTempDir;
			if (FarMkTempEx(strTempDir) && apiCreateDirectory(strTempDir,NULL))
      {
        string strSaveDir;
				apiGetCurrentDirectory(strSaveDir);

        string strDizName=strTempDir+L"\\"+DestPanel->strPluginDizName;

				DestDiz->Flush(L"",strDizName);
        if (Move)
					SrcDiz->Flush(L"");
				PluginPanelItem PanelItem;

        if (FileNameToPluginItem(strDizName,&PanelItem))
          CtrlObject->Plugins.PutFiles(DestPanel->hPlugin,&PanelItem,1,FALSE,OPM_SILENT|OPM_DESCR);
        else
          if (Delete)
          {
            PluginPanelItem pi;
            memset(&pi,0,sizeof(pi));

            pi.FindData.lpwszFileName = xf_wcsdup (DestPanel->strPluginDizName);
            CtrlObject->Plugins.DeleteFiles(DestPanel->hPlugin,&pi,1,OPM_SILENT);
            xf_free (pi.FindData.lpwszFileName);
          }
        FarChDir(strSaveDir);
        DeleteFileWithFolder(strDizName);
      }
    }
  }
}


void FileList::PluginGetFiles(const wchar_t **DestPath,int Move)
{
  _ALGO(CleverSysLog clv(L"FileList::PluginGetFiles()"));
	PluginPanelItem *ItemList, *PList;
  int ItemNumber;
  SaveSelection();
  CreatePluginItemList(ItemList,ItemNumber);

  if (ItemList!=NULL && ItemNumber>0)
  {
    int GetCode=CtrlObject->Plugins.GetFiles(hPlugin,ItemList,ItemNumber,Move,DestPath,0);
    if ((Opt.Diz.UpdateMode==DIZ_UPDATE_IF_DISPLAYED && IsDizDisplayed()) ||
        Opt.Diz.UpdateMode==DIZ_UPDATE_ALWAYS)
    {
      DizList DestDiz;
      int DizFound=FALSE;
      PList=ItemList;
      for (int I=0;I<ItemNumber;I++,PList++)
        if (PList->Flags & PPIF_PROCESSDESCR)
        {
          if (!DizFound)
          {
            CtrlObject->Cp()->LeftPanel->ReadDiz();
            CtrlObject->Cp()->RightPanel->ReadDiz();
            DestDiz.Read(*DestPath);
            DizFound=TRUE;
          }
          string strName = PList->FindData.lpwszFileName;
          string strShortName = PList->FindData.lpwszAlternateFileName;
          CopyDiz(strName,strShortName,strName,strName,&DestDiz);
        }
      DestDiz.Flush(*DestPath);
    }
    if (GetCode==1)
    {
      if (!ReturnCurrentFile)
        ClearSelection();
      if (Move)
      {
        SetPluginModified();
        PutDizToPlugin(this,ItemList,ItemNumber,TRUE,FALSE,NULL,&Diz);
      }
    }
    else
      if (!ReturnCurrentFile)
        PluginClearSelection(ItemList,ItemNumber);
    DeletePluginItemList(ItemList,ItemNumber);
    Update(UPDATE_KEEP_SELECTION);
    Redraw();
    Panel *AnotherPanel=CtrlObject->Cp()->GetAnotherPanel(this);
    AnotherPanel->Update(UPDATE_KEEP_SELECTION|UPDATE_SECONDARY);
    AnotherPanel->Redraw();
  }
}


void FileList::PluginToPluginFiles(int Move)
{
  _ALGO(CleverSysLog clv(L"FileList::PluginToPluginFiles()"));
	PluginPanelItem *ItemList;
  int ItemNumber;
  Panel *AnotherPanel=CtrlObject->Cp()->GetAnotherPanel(this);
  string strTempDir;
  if (AnotherPanel->GetMode()!=PLUGIN_PANEL)
    return;
  FileList *AnotherFilePanel=(FileList *)AnotherPanel;

  if (!FarMkTempEx(strTempDir))
    return;
  SaveSelection();
	apiCreateDirectory(strTempDir,NULL);
  CreatePluginItemList(ItemList,ItemNumber);
  if (ItemList!=NULL && ItemNumber>0)
  {
    const wchar_t *lpwszTempDir=strTempDir;
    int PutCode=CtrlObject->Plugins.GetFiles(hPlugin,ItemList,ItemNumber,FALSE,&lpwszTempDir,OPM_SILENT);
    strTempDir=lpwszTempDir;
    if (PutCode==1 || PutCode==2)
    {
      string strSaveDir;
			apiGetCurrentDirectory(strSaveDir);

      FarChDir(strTempDir);
      PutCode=CtrlObject->Plugins.PutFiles(AnotherFilePanel->hPlugin,ItemList,ItemNumber,FALSE,0);
      if (PutCode==1 || PutCode==2)
      {
        if (!ReturnCurrentFile)
          ClearSelection();
        AnotherPanel->SetPluginModified();
        PutDizToPlugin(AnotherFilePanel,ItemList,ItemNumber,FALSE,FALSE,&Diz,&AnotherFilePanel->Diz);
        if (Move)
          if (CtrlObject->Plugins.DeleteFiles(hPlugin,ItemList,ItemNumber,OPM_SILENT))
          {
            SetPluginModified();
            PutDizToPlugin(this,ItemList,ItemNumber,TRUE,FALSE,NULL,&Diz);
          }
      }
      else
        if (!ReturnCurrentFile)
          PluginClearSelection(ItemList,ItemNumber);
      FarChDir(strSaveDir);
    }
    DeleteDirTree(strTempDir);
    DeletePluginItemList(ItemList,ItemNumber);
    Update(UPDATE_KEEP_SELECTION);
    Redraw();
    if (PanelMode==PLUGIN_PANEL)
      AnotherPanel->Update(UPDATE_KEEP_SELECTION|UPDATE_SECONDARY);
    else
      AnotherPanel->Update(UPDATE_KEEP_SELECTION);
    AnotherPanel->Redraw();
  }
}


void FileList::PluginHostGetFiles()
{
  Panel *AnotherPanel=CtrlObject->Cp()->GetAnotherPanel(this);
  string strDestPath;
  string strSelName;

  DWORD FileAttr;

  SaveSelection();

  GetSelName(NULL,FileAttr);
  if (!GetSelName(&strSelName,FileAttr))
    return;

  AnotherPanel->GetCurDir(strDestPath);
  if (((!AnotherPanel->IsVisible() || AnotherPanel->GetType()!=FILE_PANEL) &&
      SelFileCount==0) || strDestPath.IsEmpty() )
  {
    strDestPath = PointToName(strSelName);
    // SVS: � ����� ����� ����� ����� ����� � ������?
    size_t pos;
    if (strDestPath.RPos(pos,L'.'))
      strDestPath.SetLength(pos);
  }

  int OpMode=OPM_TOPLEVEL,ExitLoop=FALSE;
  GetSelName(NULL,FileAttr);
  while (!ExitLoop && GetSelName(&strSelName,FileAttr))
  {
    HANDLE hCurPlugin;

    if ((hCurPlugin=OpenPluginForFile(strSelName,FileAttr))!=INVALID_HANDLE_VALUE &&
        hCurPlugin!=(HANDLE)-2)
    {
			PluginPanelItem *ItemList;
      int ItemNumber;
      _ALGO(SysLog(L"call Plugins.GetFindData()"));
      if (CtrlObject->Plugins.GetFindData(hCurPlugin,&ItemList,&ItemNumber,0))
      {
        _ALGO(SysLog(L"call Plugins.GetFiles()"));
        const wchar_t *lpwszDestPath=strDestPath;
        ExitLoop=CtrlObject->Plugins.GetFiles(hCurPlugin,ItemList,ItemNumber,FALSE,&lpwszDestPath,OpMode)!=1;
        strDestPath=lpwszDestPath;
        if (!ExitLoop)
        {
          _ALGO(SysLog(L"call ClearLastGetSelection()"));
          ClearLastGetSelection();
        }
        _ALGO(SysLog(L"call Plugins.FreeFindData()"));
        CtrlObject->Plugins.FreeFindData(hCurPlugin,ItemList,ItemNumber);
        OpMode|=OPM_SILENT;
      }
      _ALGO(SysLog(L"call Plugins.ClosePlugin"));
      CtrlObject->Plugins.ClosePlugin(hCurPlugin);
    }
  }
  Update(UPDATE_KEEP_SELECTION);
  Redraw();
  AnotherPanel->Update(UPDATE_KEEP_SELECTION|UPDATE_SECONDARY);
  AnotherPanel->Redraw();
}


void FileList::PluginPutFilesToNew()
{
  _ALGO(CleverSysLog clv(L"FileList::PluginPutFilesToNew()"));
  //_ALGO(SysLog(L"FileName='%s'",(FileName?FileName:"(NULL)")));
  _ALGO(SysLog(L"call Plugins.OpenFilePlugin(NULL,NULL,0)"));
  HANDLE hNewPlugin=CtrlObject->Plugins.OpenFilePlugin(NULL,NULL,0,0);
  if (hNewPlugin!=INVALID_HANDLE_VALUE && hNewPlugin!=(HANDLE)-2)
  {
    _ALGO(SysLog(L"Create: FileList TmpPanel, FileCount=%d",FileCount));
    FileList TmpPanel;
    TmpPanel.SetPluginMode(hNewPlugin,L"");  // SendOnFocus??? true???
    TmpPanel.SetModalMode(TRUE);
    int PrevFileCount=FileCount;
    /* $ 12.04.2002 IS
       ���� PluginPutFilesToAnother ������� �����, �������� �� 2, �� �����
       ����������� ���������� ������ �� ��������� ����.
    */
    int rc=PluginPutFilesToAnother(FALSE,&TmpPanel);
    if (rc!=2 && FileCount==PrevFileCount+1)
    {
      int LastPos=0;
      /* �����, ��� ����������� ���������� ��������������� �����
         ���������������� ���������� �� ���� � ������������ �����
         �������� �����. ������, ���� �����-�� ������� �������� ������
         � ������� �������� ����� � ����� �������� ������� �������,
         �� ����������� ���������������� �� ����������!
      */
			FileListItem *PtrListData, *PtrLastPos=ListData[0];
      for (int I=1; I < FileCount; I++)
      {
        PtrListData = ListData[I];
        if (FileTimeDifference(&PtrListData->CreationTime,&PtrLastPos->CreationTime) > 0)
        {
          PtrLastPos=ListData[LastPos=I];
        }
      }
      CurFile=LastPos;
      Redraw();
    }
  }
}


/* $ 12.04.2002 IS
     PluginPutFilesToAnother ������ int - ���������� ��, ��� ����������
     PutFiles:
     -1 - �������� ������������
      0 - �������
      1 - �����
      2 - �����, ������ ������������� ���������� �� ���� � ������ ���
          ������������� �� ����� (��. PluginPutFilesToNew)
*/
int FileList::PluginPutFilesToAnother(int Move,Panel *AnotherPanel)
{
  if (AnotherPanel->GetMode()!=PLUGIN_PANEL)
    return 0;
  FileList *AnotherFilePanel=(FileList *)AnotherPanel;
	PluginPanelItem *ItemList;
  int ItemNumber,PutCode=0;
  SaveSelection();
  CreatePluginItemList(ItemList,ItemNumber);
  if (ItemList!=NULL && ItemNumber>0)
  {
    SetCurPath();
    _ALGO(SysLog(L"call Plugins.PutFiles"));
    PutCode=CtrlObject->Plugins.PutFiles(AnotherFilePanel->hPlugin,ItemList,ItemNumber,Move,0);
    if (PutCode==1 || PutCode==2)
    {
      if (!ReturnCurrentFile)
      {
        _ALGO(SysLog(L"call ClearSelection()"));
        ClearSelection();
      }
      _ALGO(SysLog(L"call PutDizToPlugin"));
      PutDizToPlugin(AnotherFilePanel,ItemList,ItemNumber,FALSE,Move,&Diz,&AnotherFilePanel->Diz);
      AnotherPanel->SetPluginModified();
    }
    else
      if (!ReturnCurrentFile)
        PluginClearSelection(ItemList,ItemNumber);
    _ALGO(SysLog(L"call DeletePluginItemList"));
    DeletePluginItemList(ItemList,ItemNumber);
    Update(UPDATE_KEEP_SELECTION);
    Redraw();
    if (AnotherPanel==CtrlObject->Cp()->GetAnotherPanel(this))
    {
      AnotherPanel->Update(UPDATE_KEEP_SELECTION);
      AnotherPanel->Redraw();
    }
  }
  return PutCode;
}


void FileList::GetOpenPluginInfo(OpenPluginInfo *Info)
{
  _ALGO(CleverSysLog clv(L"FileList::GetOpenPluginInfo()"));
  //_ALGO(SysLog(L"FileName='%s'",(FileName?FileName:"(NULL)")));
  memset(Info,0,sizeof(*Info));
  if (PanelMode==PLUGIN_PANEL)
    CtrlObject->Plugins.GetOpenPluginInfo(hPlugin,Info);
}


/*
   ������� ��� ������ ������� "�������� �������" (Shift-F3)
*/
void FileList::ProcessHostFile()
{
  _ALGO(CleverSysLog clv(L"FileList::ProcessHostFile()"));
  //_ALGO(SysLog(L"FileName='%s'",(FileName?FileName:"(NULL)")));
  if (FileCount>0 && SetCurPath())
  {
    int Done=FALSE;

    SaveSelection();

    if (PanelMode==PLUGIN_PANEL && !PluginsStack[PluginsStackSize-1]->strHostFile.IsEmpty() )
    {
			PluginPanelItem *ItemList;
      int ItemNumber;
      _ALGO(SysLog(L"call CreatePluginItemList"));
      CreatePluginItemList(ItemList,ItemNumber);
      _ALGO(SysLog(L"call Plugins.ProcessHostFile"));
      Done=CtrlObject->Plugins.ProcessHostFile(hPlugin,ItemList,ItemNumber,0);
      if (Done)
        SetPluginModified();
      else
      {
        if (!ReturnCurrentFile)
          PluginClearSelection(ItemList,ItemNumber);
        Redraw();
      }
      _ALGO(SysLog(L"call DeletePluginItemList"));
      DeletePluginItemList(ItemList,ItemNumber);
      if (Done)
        ClearSelection();
    }
    else
    {
      int SCount=GetRealSelCount();
      if(SCount > 0)
      {
        for(int I=0; I < FileCount; ++I)
        {
					if(ListData[I]->Selected)
          {
            Done=ProcessOneHostFile(I);
            if(Done == 1)
							Select(ListData[I],0);
            else if(Done == -1)
              continue;
            else       // ���� ��� ������, ��... ����� ���� ESC �� ������ ������
              break;   //
          }
        }

        if (SelectedFirst)
          SortFileList(TRUE);
      }
      else
      {
        if((Done=ProcessOneHostFile(CurFile)) == 1)
         ClearSelection();
      }
    }

    if (Done)
    {
      Update(UPDATE_KEEP_SELECTION);
      Redraw();
      Panel *AnotherPanel=CtrlObject->Cp()->GetAnotherPanel(this);
      AnotherPanel->Update(UPDATE_KEEP_SELECTION|UPDATE_SECONDARY);
      AnotherPanel->Redraw();
    }
  }
}

/*
  ��������� ������ ����-�����.
  Return:
    -1 - ���� ���� ������� �������� �� ���������
     0 - ������ ������ FALSE
     1 - ������ ������ TRUE
*/
int FileList::ProcessOneHostFile(int Idx)
{
  _ALGO(CleverSysLog clv(L"FileList::ProcessOneHostFile()"));
  int Done=-1;

  _ALGO(SysLog(L"call OpenPluginForFile([Idx=%d] '%s')",Idx,ListData[Idx]->Name));

  string strName = ListData[Idx]->strName;

  HANDLE hNewPlugin=OpenPluginForFile(strName,ListData[Idx]->FileAttr);

  if (hNewPlugin!=INVALID_HANDLE_VALUE && hNewPlugin!=(HANDLE)-2)
  {
		PluginPanelItem *ItemList;
    int ItemNumber;
    _ALGO(SysLog(L"call Plugins.GetFindData"));
    if (CtrlObject->Plugins.GetFindData(hNewPlugin,&ItemList,&ItemNumber,OPM_TOPLEVEL))
    {
      _ALGO(SysLog(L"call Plugins.ProcessHostFile"));
      Done=CtrlObject->Plugins.ProcessHostFile(hNewPlugin,ItemList,ItemNumber,OPM_TOPLEVEL);
      _ALGO(SysLog(L"call Plugins.FreeFindData"));
      CtrlObject->Plugins.FreeFindData(hNewPlugin,ItemList,ItemNumber);
    }
    _ALGO(SysLog(L"call Plugins.ClosePlugin"));
    CtrlObject->Plugins.ClosePlugin(hNewPlugin);
  }
  return Done;
}



void FileList::SetPluginMode(HANDLE hPlugin,const wchar_t *PluginFile,bool SendOnFocus)
{
  if (PanelMode!=PLUGIN_PANEL)
  {
    CtrlObject->FolderHistory->AddToHistory(strCurDir,NULL,0);
  }

  PushPlugin(hPlugin,PluginFile);

  FileList::hPlugin=hPlugin;

  PanelMode=PLUGIN_PANEL;
  if(SendOnFocus)
    SetFocus();
	OpenPluginInfo Info;
  CtrlObject->Plugins.GetOpenPluginInfo(hPlugin,&Info);
  if (Info.StartPanelMode)
    SetViewMode(VIEW_0+Info.StartPanelMode-L'0');
  CtrlObject->Cp()->RedrawKeyBar();
  if (Info.StartSortMode)
  {
    SortMode=Info.StartSortMode-(SM_UNSORTED-UNSORTED);
    SortOrder=Info.StartSortOrder ? -1:1;
  }
  Panel *AnotherPanel=CtrlObject->Cp()->GetAnotherPanel(this);
  if (AnotherPanel->GetType()!=FILE_PANEL)
  {
    AnotherPanel->Update(UPDATE_KEEP_SELECTION);
    AnotherPanel->Redraw();
  }
}

void FileList::PluginGetPanelInfo(PanelInfo &Info)
{
	CorrectPosition();
	Info.CurrentItem=CurFile;
	Info.TopPanelItem=CurTopFile;
	Info.ShortNames=ShowShortNames;
	Info.ItemsNumber=FileCount;
	Info.SelectedItemsNumber=GetSelCount();
	if(Info.SelectedItemsNumber==1 && TestParentFolderName(ListData[CurFile]->strName))
		Info.SelectedItemsNumber=0;
}

size_t FileList::PluginGetPanelItem(int ItemNumber,PluginPanelItem *Item)
{
	size_t result=0;
	if(ListData && ItemNumber<FileCount)
	{
		result=FileListToPluginItem2(ListData[ItemNumber],Item);
	}
	return result;
}

size_t FileList::PluginGetSelectedPanelItem(int ItemNumber,PluginPanelItem *Item)
{
	size_t result=0;
	if(ListData && ItemNumber<FileCount)
	{
		if(ItemNumber==CacheSelIndex)
		{
			result=FileListToPluginItem2(ListData[CacheSelPos],Item);
		}
		else
		{
			if(ItemNumber<CacheSelIndex) CacheSelIndex=-1;
			int CurSel=CacheSelIndex,StartValue=CacheSelIndex>=0?CacheSelPos+1:0;
			for(int i=StartValue;i<FileCount;i++)
			{
				if(ListData[i]->Selected)
					CurSel++;
				if(CurSel==ItemNumber)
				{
					result=FileListToPluginItem2(ListData[i],Item);
					CacheSelIndex=ItemNumber;
					CacheSelPos=i;
					break;
				}
			}
			if(CurSel==-1 && !ItemNumber)
			{
				result=FileListToPluginItem2(ListData[CurFile],Item);
				CacheSelIndex=-1;
			}
		}
	}
	return result;
}

void FileList::PluginGetColumnTypesAndWidths(string& strColumnTypes,string& strColumnWidths)
{
	ViewSettingsToText(ViewSettings.ColumnType,ViewSettings.ColumnWidth,ViewSettings.ColumnWidthType,
		ViewSettings.ColumnCount,strColumnTypes,strColumnWidths);
}

void FileList::PluginBeginSelection()
{
	SaveSelection();
}

void FileList::PluginSetSelection(int ItemNumber,bool Selection)
{
	Select(ListData[ItemNumber],Selection);
}

void FileList::PluginEndSelection()
{
	if(SelectedFirst)
	{
		SortFileList(TRUE);
	}
}

void FileList::ProcessPluginCommand()
{
  _ALGO(CleverSysLog clv(L"FileList::ProcessPluginCommand"));
  _ALGO(SysLog(L"PanelMode=%s",(PanelMode==PLUGIN_PANEL?"PLUGIN_PANEL":"NORMAL_PANEL")));
  int Command=PluginCommand;
  PluginCommand=-1;
  if (PanelMode==PLUGIN_PANEL)
    switch(Command)
    {
      case FCTL_CLOSEPLUGIN:
        _ALGO(SysLog(L"Command=FCTL_CLOSEPLUGIN"));
				SetCurDir(strPluginParam,TRUE);
        if(strPluginParam.IsEmpty())
          Update(UPDATE_KEEP_SELECTION);
        Redraw();
        break;
    }
}

void FileList::SetPluginModified()
{
  if (PluginsStackSize>0)
    PluginsStack[PluginsStackSize-1]->Modified=TRUE;
}


HANDLE FileList::GetPluginHandle()
{
  return(hPlugin);
}


int FileList::ProcessPluginEvent(int Event,void *Param)
{
  if (PanelMode==PLUGIN_PANEL)
    return(CtrlObject->Plugins.ProcessEvent(hPlugin,Event,Param));
  return(FALSE);
}


void FileList::PluginClearSelection(PluginPanelItem *ItemList,int ItemNumber)
{
  SaveSelection();

  int FileNumber=0,PluginNumber=0;
  while (PluginNumber<ItemNumber)
  {
		PluginPanelItem *CurPluginPtr=ItemList+PluginNumber;
    if ((CurPluginPtr->Flags & PPIF_SELECTED)==0)
    {
      while (StrCmpI(CurPluginPtr->FindData.lpwszFileName,ListData[FileNumber]->strName)!=0)
        if (++FileNumber>=FileCount)
          return;
      Select(ListData[FileNumber++],0);
    }
    PluginNumber++;
  }
}
