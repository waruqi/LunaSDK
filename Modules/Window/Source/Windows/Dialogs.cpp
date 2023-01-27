/*
* This file is a portion of Luna SDK.
* For conditions of distribution and use, see the disclaimer
* and license in LICENSE.txt
* 
* @file Dialogs.cpp
* @author JXMaster
* @date 2022/6/6
*/
#include <Runtime/PlatformDefines.hpp>

#ifdef LUNA_PLATFORM_WINDOWS
#define LUNA_WINDOW_API LUNA_EXPORT
#include "../../MessageBox.hpp"
#include "../../FileDialog.hpp"
#include "../../Window.hpp"
#include <Runtime/Unicode.hpp>

#include <Runtime/Platform/Windows/MiniWin.hpp>
#include <commdlg.h>
#include <wrl/client.h>
#include <ShlObj.h>

#pragma comment (lib, "Shcore.lib")
#pragma comment (lib, "Ole32.lib")
#pragma comment (lib, "Comdlg32.lib")

using Microsoft::WRL::ComPtr;

namespace Luna
{
	namespace Window
	{
		LUNA_WINDOW_API R<MessageBoxButton> message_box(const c8* text, const c8* caption, MessageBoxType type, MessageBoxIcon icon)
		{
			wchar_t* wtext;
			wchar_t* wcap;
			usize text_size, caption_size;
			text_size = utf8_to_utf16_len(text);
			caption_size = utf8_to_utf16_len(caption);
			wtext = (wchar_t*)alloca((text_size + 1) * sizeof(wchar_t));
			wcap = (wchar_t*)alloca((caption_size + 1) * sizeof(wchar_t));
			utf8_to_utf16((char16_t*)wtext, text_size + 1, text);
			utf8_to_utf16((char16_t*)wcap, caption_size + 1, caption);
			UINT f = 0;
			switch (type)
			{
			case MessageBoxType::ok:
				f = MB_OK;
				break;
			case MessageBoxType::ok_cancel:
				f = MB_OKCANCEL;
				break;
			case MessageBoxType::retry_cancel:
				f = MB_RETRYCANCEL;
				break;
			case MessageBoxType::yes_no:
				f = MB_YESNO;
				break;
			case MessageBoxType::yes_no_cancel:
				f = MB_YESNOCANCEL;
				break;
			default:
				lupanic();
				break;
			}
			switch (icon)
			{
			case MessageBoxIcon::none:
				break;
			case MessageBoxIcon::information:
				f |= MB_ICONINFORMATION;
				break;
			case MessageBoxIcon::warning:
				f |= MB_ICONWARNING;
				break;
			case MessageBoxIcon::question:
				f |= MB_ICONQUESTION;
				break;
			case MessageBoxIcon::error:
				f |= MB_ICONERROR;
				break;
			default:
				lupanic();
				break;
			}
			int ret = ::MessageBoxW(NULL, wtext, wcap, f);
			if (!ret)
			{
				return BasicError::bad_platform_call();
			}
			switch (ret)
			{
			case IDOK:
				return MessageBoxButton::ok;
			case IDNO:
				return MessageBoxButton::no;
			case IDYES:
				return MessageBoxButton::yes;
			case IDCANCEL:
				return MessageBoxButton::cancel;
			case IDRETRY:
				return MessageBoxButton::retry;
			default:
				lupanic();
			}
			return MessageBoxButton::ok;
		}

		LUNA_WINDOW_API R<Vector<Path>> open_file_dialog(const c8* filter, const c8* title, const Path& initial_dir, FileOpenDialogFlag flags)
		{
			Vector<Path> paths;
			OPENFILENAMEW ofn;
			ZeroMemory(&ofn, sizeof(ofn));
			ofn.lStructSize = sizeof(ofn);
			ofn.hwndOwner = NULL;
			wchar_t out[2048];
			if (initial_dir != Path())
			{
				auto path_str = initial_dir.encode(PathSeparator::back_slash, true);
				utf8_to_utf16((char16_t*)out, 2048, path_str.c_str());
			}
			else
			{
				out[0] = '\0';
			}

			wchar_t* wtitle = nullptr;
			if (title)
			{
				usize wt_size = utf8_to_utf16_len(title);
				wtitle = (wchar_t*)alloca(sizeof(wchar_t) * (wt_size + 1));
				utf8_to_utf16((char16_t*)wtitle, wt_size + 1, title);
			}

			const char* cur_filter = filter;
			usize filter_wsize = 0;
			while (*cur_filter)
			{
				usize len = strlen(cur_filter);
				usize wlen = utf8_to_utf16_len(cur_filter);
				filter_wsize += wlen + 1;
				cur_filter += len + 1;
			}
			wchar_t* wfilter = (wchar_t*)alloca((filter_wsize + 1) * sizeof(wchar_t));
			cur_filter = filter;
			usize wfilter_cur = 0;
			while (*cur_filter)
			{
				usize len = strlen(cur_filter);
				usize outputted = utf8_to_utf16((char16_t*)wfilter + wfilter_cur, filter_wsize - wfilter_cur + 1, cur_filter);
				cur_filter += len + 1;
				wfilter_cur += outputted + 1;
			}
			wfilter[filter_wsize] = '\0';
			ofn.lpstrFile = out;
			ofn.nMaxFile = 2048 * sizeof(wchar_t);
			ofn.lpstrFilter = wfilter;
			ofn.nFilterIndex = 1;
			ofn.lpstrFileTitle = NULL;
			ofn.nMaxFileTitle = 0;
			ofn.lpstrInitialDir = nullptr;
			ofn.lpstrTitle = wtitle;
			ofn.Flags = OFN_PATHMUSTEXIST | OFN_FILEMUSTEXIST | OFN_NOCHANGEDIR | OFN_NONETWORKBUTTON;
			if ((flags & FileOpenDialogFlag::multi_select) != FileOpenDialogFlag::none)
			{
				ofn.Flags |= OFN_ALLOWMULTISELECT;
			}
			bool res = ::GetOpenFileNameW(&ofn) != FALSE;
			if (res)
			{
				// Checks if multiple files are selected.
				if (ofn.lpstrFile[ofn.nFileOffset - 1] == '\0')
				{
					// Multiple file.
					char* ret_buf = (char*)alloca(sizeof(char) * 1024);
					usize dir_sz = utf16_to_utf8(ret_buf, 1024, (char16_t*)ofn.lpstrFile);
					char16_t* wdir_cur = (char16_t*)ofn.lpstrFile + strlen16((char16_t*)ofn.lpstrFile) + 1;
					while (wdir_cur)
					{
						utf16_to_utf8(ret_buf + dir_sz, 1024 - dir_sz, wdir_cur);
						wdir_cur += strlen16(wdir_cur) + 1;
						auto ret_path = Path(ret_buf);
						paths.push_back(ret_path);
					}
				}
				else
				{
					// Single file.
					usize ret_sz = utf16_to_utf8_len((char16_t*)ofn.lpstrFile);
					char* ret_buf = (char*)alloca(sizeof(char) * (ret_sz + 1));
					utf16_to_utf8(ret_buf, (ret_sz + 1), (char16_t*)ofn.lpstrFile);
					auto ret_path = Path(ret_buf);
					paths.push_back(ret_path);
				}
			}
			else
			{
				DWORD err_code = CommDlgExtendedError();
				if (err_code == 0)
				{
					return BasicError::interrupted();
				}
				return set_error(BasicError::bad_platform_call(), "Open File Dialog (common dialog box) reports failure, error code: %u", err_code);
			}

			return paths;
		}

		LUNA_WINDOW_API R<Path> save_file_dialog(const c8* filter, const c8* title, const Path& initial_file_path)
		{
			Path ret_path;
			// Translate filter.
			const char* cur_filter = filter;
			usize filter_wsize = 0;
			while (*cur_filter)
			{
				usize len = strlen(cur_filter);
				usize wlen = utf8_to_utf16_len(cur_filter);
				filter_wsize += wlen + 1;
				cur_filter += len + 1;
			}

			wchar_t* wtitle = nullptr;
			if (title)
			{
				usize wt_size = utf8_to_utf16_len(title);
				wtitle = (wchar_t*)alloca(sizeof(wchar_t) * (wt_size + 1));
				utf8_to_utf16((char16_t*)wtitle, wt_size + 1, title);
			}

			wchar_t* wfilter = (wchar_t*)alloca((filter_wsize + 1) * sizeof(wchar_t));
			cur_filter = filter;
			usize wfilter_cur = 0;
			while (*cur_filter)
			{
				usize len = strlen(cur_filter);
				usize outputted = utf8_to_utf16((char16_t*)wfilter + wfilter_cur, filter_wsize - wfilter_cur + 1, cur_filter);
				cur_filter += len + 1;
				wfilter_cur += outputted + 1;
			}
			wfilter[filter_wsize] = '\0';
			// Translate initial path if have.
			wchar_t out[1024];
			if (initial_file_path != Path())
			{
				auto path_str = initial_file_path.encode(PathSeparator::back_slash, true);
				utf8_to_utf16((char16_t*)out, 1024, path_str.c_str());
			}
			else
			{
				out[0] = '\0';
			}
			OPENFILENAMEW ofn;
			ZeroMemory(&ofn, sizeof(ofn));
			ofn.lStructSize = sizeof(ofn);
			ofn.hwndOwner = NULL;
			ofn.lpstrFile = out;
			ofn.nMaxFile = 1024;
			ofn.lpstrFilter = wfilter;
			ofn.nFilterIndex = 1;
			ofn.lpstrDefExt = NULL;
			ofn.lpstrFileTitle = NULL;
			ofn.nMaxFileTitle = 0;
			ofn.lpstrInitialDir = NULL;
			ofn.lpstrTitle = wtitle;
			ofn.Flags = OFN_OVERWRITEPROMPT | OFN_NOCHANGEDIR | OFN_NONETWORKBUTTON;

			bool res = GetSaveFileNameW(&ofn) != FALSE;

			if (!res)
			{
				DWORD err_code = CommDlgExtendedError();
				if (err_code == 0)
				{
					return BasicError::interrupted();
				}
				return set_error(BasicError::bad_platform_call(), "Save File Dialog (common dialog box) reports failure, error code: %u", err_code);
			}

			usize ret_sz = utf16_to_utf8_len((char16_t*)ofn.lpstrFile);
			char* ret_buf = (char*)alloca(sizeof(char) * (ret_sz + 1));
			utf16_to_utf8(ret_buf, (ret_sz + 1), (char16_t*)ofn.lpstrFile);
			ret_path = Path(ret_buf);
			return ret_path;
		}

		LUNA_WINDOW_API R<Path> open_dir_dialog(const c8* title, const Path& initial_dir)
		{
			Path path;
			ComPtr<IFileDialog> pfd;
			if (FAILED(CoCreateInstance(CLSID_FileOpenDialog, NULL, CLSCTX_INPROC_SERVER, IID_PPV_ARGS(&pfd))))
			{
				return BasicError::bad_platform_call();
			}

			if (title)
			{
				wchar_t* wtitle = nullptr;
				usize wt_size = utf8_to_utf16_len(title);
				wtitle = (wchar_t*)alloca(sizeof(wchar_t) * (wt_size + 1));
				utf8_to_utf16((char16_t*)wtitle, wt_size + 1, title);
				pfd->SetTitle(wtitle);
			}

			if (initial_dir != Path())
			{
				PIDLIST_ABSOLUTE pidl;
				WCHAR wstarting_dir[MAX_PATH];
				auto initial_str = initial_dir.encode(PathSeparator::back_slash, true);
				utf8_to_utf16((char16_t*)wstarting_dir, MAX_PATH, initial_str.c_str());
				HRESULT hresult = ::SHParseDisplayName(wstarting_dir, 0, &pidl, SFGAO_FOLDER, 0);
				if (FAILED(hresult))
				{
					return BasicError::bad_platform_call();
				}
				ComPtr<IShellItem> psi;
				hresult = ::SHCreateShellItem(NULL, NULL, pidl, &psi);
				if (SUCCEEDED(hresult))
				{
					pfd->SetFolder(psi.Get());
				}
				ILFree(pidl);
			}

			DWORD dwOptions;
			if (SUCCEEDED(pfd->GetOptions(&dwOptions)))
			{
				pfd->SetOptions(dwOptions | FOS_PICKFOLDERS);
			}

			if (SUCCEEDED(pfd->Show(NULL)))
			{
				ComPtr<IShellItem> psi;
				if (SUCCEEDED(pfd->GetResult(&psi)))
				{
					WCHAR* tmp;
					if (SUCCEEDED(psi->GetDisplayName(SIGDN_DESKTOPABSOLUTEPARSING, &tmp)))
					{
						usize path_len = utf16_to_utf8_len((char16_t*)tmp);
						char* buf = (char*)alloca(sizeof(char) * (path_len + 1));
						utf16_to_utf8(buf, path_len + 1, (char16_t*)tmp);

						path = Path(buf);

						CoTaskMemFree(tmp);
					}
				}
			}
			if (path == Path())
			{
				return BasicError::bad_platform_call();
			}
			return path;
		}
	}
}

#endif