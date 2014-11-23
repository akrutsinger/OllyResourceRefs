/*******************************************************************************
 * OllyResourceRefs - OllyResourceRefs.c
 *
 * By Austyn Krutsinger
 *
 ******************************************************************************/

/*******************************************************************************
 * Things to change as I think of them...
 * [ ] = To do
 * [?] = Might be a good idea?
 * [!] = Implemented
 * [+] = Added
 * [-] = Removed
 * [*] = Changed
 * [~] = Almost there...
 *
 *
 * Version 0.1.0 (21NOV2014)
 * [+] Initial release
 *
 *
 * -----------------------------------------------------------------------------
 * TODO
 * -----------------------------------------------------------------------------
 *
 * [+] Finish
 *
 *
 ******************************************************************************/

#include "OllyResourceRefs.h"
#include <stdio.h>
#include <ctype.h>
#include "plugin.h"

/* Module specific globals */
HINSTANCE	plugin_inst_g		= NULL;
t_table		logtable 			= {{0}};		/* log table used for reference data */
t_module	*current_module		= NULL;

// The predefined resource types
wchar_t *SzResourceTypes[] = {
	L"???_0",
	L"CURSOR",
	L"BITMAP",
	L"ICON",
	L"MENU",
	L"DIALOG",
	L"STRING",
	L"FONTDIR",
	L"FONT",
	L"ACCELERATORS",
	L"RCDATA",
	L"MESSAGETABLE",
	L"GROUP_CURSOR",
	L"???_13",
	L"GROUP_ICON",
	L"???_15",
	L"VERSION",
	L"DLGINCLUDE",
	L"???_18",
	L"PLUGPLAY",
	L"VXD",
	L"ANICURSOR",
	L"ANIICON"
};

IMAGE_RESOURCE_DIRECTORY_ENTRY *pStrResEntries = 0;
IMAGE_RESOURCE_DIRECTORY_ENTRY *pDlgResEntries = 0;
IMAGE_RESOURCE_DIRECTORY_ENTRY *pMnuResEntries = 0;
DWORD cStrResEntries = 0;
DWORD cDlgResEntries = 0;
DWORD cMnuResEntries = 0;
DWORD rsrc_sect_base_rva = 0;

/*
 * Plug-in menu that will appear in the main OllyDbg menu and in pop-up menu.
 */
static t_menu plugin_main_menu[] = {
	{ L"Search For Resource References",
		L"Search for references to programs resources",
		KK_DIRECT|KK_CTRL|KK_ALT|'R', menu_handler, NULL, MENU_FIND_RES_REFS },
	{ L"|About",
		L"About OllyResourceRefs",
		K_NONE, menu_handler, NULL, MENU_ABOUT },
	/* End of menu. */
	{ NULL, NULL, K_NONE, NULL, NULL, 0 }
};

/*
 * Plug-in menu that will appear in the pop-up menu.
 */
static t_menu plugin_popup_menu[] = {
	{ L"Search For Resource References",
		L"Search for references to programs resources",
		KK_DIRECT|KK_CTRL|KK_ALT|'R', menu_handler, NULL, MENU_FIND_RES_REFS },
	/* End of menu. */
	{ NULL, NULL, K_NONE, NULL, NULL, 0 }
};

/* Pop-up menu for view address in CPU Disasm */
static t_menu log_window_popup_menu[] = {
	{ L"View In CPU Disasm",
		L"View Address In CPU Disasm",
		KK_DIRECT|KK_SHIFT|'W', menu_handler, NULL, MENU_VIEW_IN_CPU },
	/* End of menu. */
	{ NULL, NULL, K_NONE, NULL, NULL, 0 }
};

/*
 *
 * Plugin specific functions
 *
 */
void about_message(void)
{
	wchar_t szMessage[TEXTLEN] = { 0 };
	wchar_t szBuffer[SHORTNAME];
	int n;

	/* Debuggee should continue execution while message box is displayed. */
	Resumeallthreads();
	/* In this case, swprintf() would be as good as a sequence of StrcopyW(), */
	/* but secure copy makes buffer overflow impossible. */
	n = StrcopyW(szMessage, TEXTLEN, L"OllyResourceRefs v");
	n += StrcopyW(szMessage + n, TEXTLEN - n, OLLYRESREFS_VERSION);
	n += StrcopyW(szMessage + n, TEXTLEN - n, L"\n\nCoded by Austyn Krutsinger <akrutsinger@gmail.com>");
	n += StrcopyW(szMessage + n, TEXTLEN - n, L"\n\nCompiled on ");
	Asciitounicode(__DATE__, SHORTNAME, szBuffer, SHORTNAME);
	n += StrcopyW(szMessage + n, TEXTLEN - n, szBuffer);
	n += StrcopyW(szMessage + n, TEXTLEN - n, L" ");
	Asciitounicode(__TIME__, SHORTNAME, szBuffer, SHORTNAME);
	n += StrcopyW(szMessage + n, TEXTLEN - n, szBuffer);
	MessageBox(hwollymain, szMessage, L"About OllyResourceRefs", MB_OK|MB_ICONINFORMATION);
	/* Suspendallthreads() and Resumeallthreads() must be paired, even if they */
	/* are called in inverse order! */
	Suspendallthreads();
}

int menu_handler(t_table* pTable, wchar_t* pszName, DWORD dwIndex, int iMode)
{
	UNREFERENCED_PARAMETER(pTable);
	UNREFERENCED_PARAMETER(pszName);

	LPLOGDATA pLogData;

	switch (iMode) {
	case MENU_VERIFY:
		return MENU_NORMAL;

	case MENU_EXECUTE:
		switch (dwIndex) {
		case MENU_FIND_RES_REFS:
			/* only execute if a module is loaded */
			if (Findmainmodule() == NULL) {
				Resumeallthreads();
				MessageBox(hwollymain, L"No module loaded", L"OllyResourceRefs", MB_OK | MB_ICONINFORMATION);
				Suspendallthreads();
			} else {
				if (logtable.hw == NULL) {
					Createtablewindow(&logtable, 0, logtable.bar.nbar, NULL, L"ICO_PLUGIN", OLLYRESREFS_NAME);
				} else {
					Activatetablewindow(&logtable);
				}
				find_resource_refs();
				return MENU_REDRAW;	/* force redrawing of the table after data is added to it */
			}
			break;
		case MENU_VIEW_IN_CPU:
			//View address in CPU Disasm
            pLogData = (LPLOGDATA)Getsortedbyselection(&pTable->sorted, pTable->sorted.selected);
            if (pLogData != NULL) {
                Setcpu(0, pLogData->address, 0, 0, 0, CPU_ASMHIST|CPU_ASMCENTER|CPU_ASMFOCUS);
            }
			Activatetablewindow(&logtable);
			break;
		case MENU_ABOUT:
			about_message();
			break;
		}
		return MENU_NOREDRAW;
	}
	return MENU_ABSENT;
}

void create_log_window(void)
{
	StrcopyW(logtable.name, SHORTNAME, OLLYRESREFS_NAME);
	logtable.mode = TABLE_SAVEPOS|TABLE_AUTOUPD;
	logtable.bar.visible = 1;

	logtable.bar.name[0] = L"Address";
	logtable.bar.expl[0] = L"";
	logtable.bar.mode[0] = BAR_SORT;
	logtable.bar.defdx[0] = 9;

	logtable.bar.name[1] = L"Command";
	logtable.bar.expl[1] = L"";
	logtable.bar.mode[1] = BAR_SORT;
	logtable.bar.defdx[1] = 10;

	logtable.bar.name[2] = L"Item Text";
	logtable.bar.expl[2] = L"";
	logtable.bar.mode[2] = BAR_SORT;
	logtable.bar.defdx[2] = 80;

	logtable.bar.name[3] = L"Resource Type";
	logtable.bar.expl[3] = L"";
	logtable.bar.mode[3] = BAR_SORT;
	logtable.bar.defdx[3] = 34;


	logtable.bar.nbar = 4;
	logtable.tabfunc = (TABFUNC*)log_window_proc;
	logtable.custommode = 0;
	logtable.customdata = NULL;
	logtable.updatefunc = NULL;
	logtable.drawfunc = (DRAWFUNC*)log_window_draw;
	logtable.tableselfunc = NULL;
	logtable.menu = (t_menu*)log_window_popup_menu;
}

long log_window_draw(wchar_t *pszBuffer, uchar *pMask, int *pSelect, t_table *pTable, t_drawheader *pHeader, int iColumn, void *pCache)
{
	int	str_len = 0;
	LPLOGDATA pLogData = (LPLOGDATA)pHeader;

	switch (iColumn)
	{
	case 0:	/* address of reference */
		str_len = Simpleaddress(pszBuffer, pLogData->address, pMask, pSelect);
		break;
	case 1: /* command that references resource */
        str_len = swprintf(pszBuffer, L"%ls", pLogData->command);
		break;
	case 2:	/* text of resource item */
		str_len = swprintf(pszBuffer, L"%ls", pLogData->item_text);
		break;
	case 3: /* type of resource */
        str_len = swprintf(pszBuffer, L"%ls", pLogData->resource_type);
		break;
	default:
		break;
	}
	return str_len;
}

long log_window_proc(t_table *pTable, HWND hWnd, UINT uMsg, WPARAM wParam, LPARAM lParam)
{
	LPLOGDATA pLogData;
	switch (uMsg)
	{
	case WM_USER_DBLCLK:
		pLogData = (LPLOGDATA)Getsortedbyselection(&pTable->sorted, pTable->sorted.selected);
		if (pLogData != NULL) {
			Setcpu(0, pLogData->address, 0, 0, 0, CPU_ASMHIST|CPU_ASMCENTER|CPU_ASMFOCUS);
		}
		Activatetablewindow(&logtable);
		return 1;
	default:
		break;
	}
	return 0;
}

int log_window_sort_proc(const t_sorthdr *pSortHeader1, const t_sorthdr *pSortHeader2, const int iSort)
{
	int i = 0;
    LPLOGDATA pLogData1	= NULL;
    LPLOGDATA pLogData2	= NULL;

    pLogData1 = (LPLOGDATA)pSortHeader1;
    pLogData2 = (LPLOGDATA)pSortHeader2;

	if (iSort == 0) {	/* sort by address */
		if (pLogData1->address < pLogData2->address) {
			i = -1;
		} else if (pLogData1->address > pLogData2->address){
			i = 1;
		}
	} else if (iSort == 1) {	/* sort by command */
		i = _wcsicmp(pLogData1->command, pLogData2->command);
	} else if (iSort == 2) {	/* sort by resource item text */
		i = _wcsicmp(pLogData1->item_text, pLogData2->item_text);
	} else if (iSort == 3) {	/* sort by resource item type */
		i = _wcsicmp(pLogData1->resource_type, pLogData2->resource_type);
	}
	/* if items match, additionally sort by address */
	if (i == 0) {
		if (pLogData1->address < pLogData2->address) {
			i = -1;
		} else if (pLogData1->address > pLogData2->address){
			i = 1;
		}
	}
	return i;
}

DWORD GetOffsetToDataFromResEntry( 	DWORD base,
									DWORD resourceBase,
									PIMAGE_RESOURCE_DIRECTORY_ENTRY pResEntry )
{
	/*
	 * The IMAGE_RESOURCE_DIRECTORY_ENTRY is gonna point to a single
	 * IMAGE_RESOURCE_DIRECTORY, which in turn will point to the
	 * IMAGE_RESOURCE_DIRECTORY_ENTRY, which in turn will point
	 * to the IMAGE_RESOURCE_DATA_ENTRY that we're really after.  In
	 * other words, traverse down a level.
	 */
	PIMAGE_RESOURCE_DIRECTORY pStupidResDir;
	pStupidResDir = (PIMAGE_RESOURCE_DIRECTORY)
                    (resourceBase + pResEntry->OffsetToDirectory);

    PIMAGE_RESOURCE_DIRECTORY_ENTRY pResDirEntry =
	    	(PIMAGE_RESOURCE_DIRECTORY_ENTRY)(pStupidResDir + 1);// PTR MATH

	PIMAGE_RESOURCE_DATA_ENTRY pResDataEntry =
			(PIMAGE_RESOURCE_DATA_ENTRY)(resourceBase +
										 pResDirEntry->OffsetToData);

	return pResDataEntry->OffsetToData;
}

void *rva_to_ptr(DWORD rva, DWORD module_base, DWORD resource_base)
{
	/* TODO: check if ptr is a valid result before return */
	void *ptr = NULL;
	/*
	 * the offsetToData is a relative virtual address relative to the to
	 * the beginning of the module. Since we have the resource section
	 * loaded into our own local memory address, we need to get the start
	 * of the string resource entry relative to the address of the
	 * allocated memory the resource section is stored in
	 */
	DWORD va_diff = (rsrc_sect_base_rva - module_base);
	ptr = (WORD *)((resource_base - va_diff) + rva);

	return ptr;
}

void dump_string_table(	DWORD module_base,
						DWORD resource_base,
						IMAGE_RESOURCE_DIRECTORY_ENTRY *pStrResEntry,
						DWORD cStrResEntries,
						push_command_list_s *cmd_list)
{
	unsigned int i;
	WORD j, k, id;
	push_command_list_s *tmp = NULL;
	LOGDATA log_data;

	for (i = 0; i < cStrResEntries; i++, pStrResEntry++) {
		DWORD offsetToData
			= GetOffsetToDataFromResEntry(module_base, resource_base, pStrResEntry);

		/* offset to data is relative to the virtual address of the module, so convert it */
		WORD *pStrEntry = (WORD *)rva_to_ptr(offsetToData, module_base, resource_base);

		if (!pStrEntry)
			break;

		id = (pStrResEntry->Name - 1) << 4;

		for (j = 0; j < 16; j++) {
			WORD len = *pStrEntry++;
			if (len != 0) {
				wchar_t szBuffer[TEXTLEN] = { 0 };
				wchar_t szMessage[TEXTLEN] = { 0 };
				int n = 0;

				for (k = 0; k < len; k++) {
					wchar_t c = (wchar_t)pStrEntry[k];

					switch (c) {
					case L'\t':
						n += StrcopyW(szMessage + n, TEXTLEN - n, L"\\t");
						break;
					case L'\r':
						n += StrcopyW(szMessage + n, TEXTLEN - n, L"\\r");
						break;
					case L'\n':
						n += StrcopyW(szMessage + n, TEXTLEN - n, L"\\n");
						break;
					default:
						wsprintf(szBuffer, L"%c", isprint(c) ? c : L'.');
						n += StrcopyW(szMessage + n, TEXTLEN - n, szBuffer);
						break;
					}
				}
				/* use the id to locate commands that push this value directly to the stack */
				/* search for commands that push the id onto the stack */
				list_for_each_entry(tmp, &cmd_list->list, list) {
					if (tmp->immediate == (id + j)) {
						log_data.address = tmp->address;
						StrcopyW(log_data.command, TEXTLEN, tmp->cmd_buf);
						StrcopyW(log_data.item_text, TEXTLEN, szMessage);
						wsprintf(szBuffer, L"String ID=%05d", (id + j));
						StrcopyW(log_data.resource_type, TEXTLEN, szBuffer);
						Addsorteddata(&(logtable.sorted), &log_data);
					}
				}
			}
			pStrEntry += len;
		}
	}
}

void dump_dialogs(	DWORD module_base,
					DWORD resource_base,
					PIMAGE_RESOURCE_DIRECTORY_ENTRY pDlgResEntry,
					DWORD cDlgResEntries,
					push_command_list_s *cmd_list)
{
	WORD i, j;
	wchar_t buffer[TEXTLEN] = { 0 };
	wchar_t message[TEXTLEN] = { 0 };
	int n = 0;
	push_command_list_s *tmp = NULL;
	LOGDATA log_data;

	for (i = 0; i < cDlgResEntries; i++, pDlgResEntry++) {
		DWORD offsetToData
			= GetOffsetToDataFromResEntry(module_base, resource_base, pDlgResEntry);

 		DWORD *pDlgStyle = (DWORD *)rva_to_ptr(offsetToData, module_base, resource_base);

		if (!pDlgStyle) {
			break;
		}

		if (HIWORD(*pDlgStyle) != 0xFFFF ) {
			/* A regular DLGTEMPLATE */
			DLGTEMPLATE * pDlgTemplate = (DLGTEMPLATE *)pDlgStyle;
			n = StrcopyW(message, TEXTLEN, L"    class: ");

			PWORD pMenu = (PWORD)(pDlgTemplate + 1);	/* ptr math! */

			/*
			 * First comes the menu
			 */
			if (*pMenu) {
				if (*pMenu == 0xFFFF) {
					pMenu++;
				} else {
					n = StrcopyW(message, TEXTLEN, L"  menu: ");
					while (*pMenu) {
						swprintf(buffer, L"%c", LOBYTE(*pMenu++));
						n += StrcopyW(message + n, TEXTLEN - n, buffer);
					}

					pMenu++;
				}
			} else {
				pMenu++;	/* Advance past the menu name */
			}

			/*
			 * Next comes the class
			 */
			PWORD pClass = pMenu;

			if (*pClass) {
				if (*pClass == 0xFFFF) {
					pClass++;
				} else {
					n = StrcopyW(message, TEXTLEN, L"  class: ");
					while (*pClass) {
						swprintf(buffer, L"%c", LOBYTE(*pClass++));
						n += StrcopyW(message + n, TEXTLEN - n, buffer);
					}
					pClass++;
				}
			} else {
				pClass++;	/* Advance past the class name */
			}

			/*
			 * Finally comes the title
			 */
			PWORD pTitle = pClass;
			if (*pTitle) {
				n = StrcopyW(message, TEXTLEN, L"  title: ");

				while (*pTitle) {
					swprintf(buffer, L"%c", LOBYTE(*pTitle++));
					n += StrcopyW(message + n, TEXTLEN - n, buffer);
				}
				pTitle++;
			} else {
				pTitle++;	/* Advance past the Title name */
			}

			PWORD pFont = pTitle;

			if (pDlgTemplate->style & DS_SETFONT) {
				swprintf(buffer, L"  Font: %u point ", *pFont++);
				n = StrcopyW(message, TEXTLEN, buffer);
				while (*pFont) {
					swprintf(buffer, L"%c", LOBYTE(*pFont++));
					n += StrcopyW(message + n, TEXTLEN - n, buffer);
				}

				pFont++;
			} else {
    	        pFont = pTitle;
	        }

			/* DLGITEMPLATE starts on a 4 byte boundary */
			LPDLGITEMTEMPLATE pDlgItemTemplate = (LPDLGITEMTEMPLATE)pFont;

			for (j = 0; j < pDlgTemplate->cdit; j++) {
				/* Control item header.... */
				pDlgItemTemplate = (DLGITEMTEMPLATE *)
									(((DWORD)pDlgItemTemplate+3) & ~3);


				/*
				 * Next comes the control's class name or ID
				 */
				PWORD pClass = (PWORD)(pDlgItemTemplate + 1);
				if (*pClass) {
					if (*pClass == 0xFFFF) {
						pClass++;	/* point to the class ordinal */
						pClass++;	/* now advance to the next class */
					} else {
						n = StrcopyW(message, TEXTLEN, L"    class: ");
						while (*pClass) {
							swprintf(buffer, L"%c", LOBYTE(*pClass++));
							n += StrcopyW(message + n, TEXTLEN - n, buffer);
						}
						pClass++;
					}
				} else {
					pClass++;
				}

				/*
				 * next comes the title
				 */
				PWORD pTitle = pClass;

				if (*pTitle) {
					if (*pTitle == 0xFFFF) {
						pTitle++;
						swprintf(buffer, L"%u", *pTitle++);
						n = StrcopyW(message, TEXTLEN, buffer);
					} else {
						n = 0;
						memset((wchar_t *)message, 0, TEXTLEN);
						while (*pTitle) {
							swprintf(buffer, L"%c", LOBYTE(*pTitle++));
							n += StrcopyW(message + n, TEXTLEN - n, buffer);
						}
						pTitle++;
					}
				}
				else {
					/* clear message buffer */
					memset((wchar_t *)message, 0, TEXTLEN);
					pTitle++;	/* Advance past the Title name */
				}

				PBYTE pCreationData = (PBYTE)(((DWORD)pTitle + 1) & 0xFFFFFFFE);

				if (*pCreationData) {
					pCreationData += *pCreationData;
				} else {
					pCreationData++;
				}

				/* add the reference item to the log window */
				/* use this id to locate commands that push this value directly to the stack */
				/* search for commands that push the id onto the stack */
				list_for_each_entry(tmp, &cmd_list->list, list) {
					if (tmp->immediate == (DWORD)pDlgItemTemplate->id) {
						log_data.address = tmp->address;
						StrcopyW(log_data.command, TEXTLEN, tmp->cmd_buf);
						StrcopyW(log_data.item_text, TEXTLEN, message);	/* assumes message still contains "title" text */
						wsprintf(buffer, L"Dialog ID=%04d, Control ID=%04d", pDlgResEntry->Id, pDlgItemTemplate->id);
						StrcopyW(log_data.resource_type, TEXTLEN, buffer);
						Addsorteddata(&(logtable.sorted), &log_data);
					}
				}

				pDlgItemTemplate = (DLGITEMTEMPLATE *)pCreationData;
			}
		} else {
			/* A DLGTEMPLATEEX */
		}
	}
}

void dump_menus(	DWORD module_base,
					DWORD resource_base,
					PIMAGE_RESOURCE_DIRECTORY_ENTRY pDlgResEntry,
					DWORD cDlgResEntries,
					push_command_list_s *cmd_list)
{

}
/* Get a Unicode string representing a resource type */
void get_resource_type_name(DWORD type, wchar_t *buffer, UINT cBytes)
{
    if (type <= (DWORD)RT_ANIICON)
        wcsncpy(buffer, SzResourceTypes[type], cBytes);
    else
        swprintf(buffer, L"%X", type);
}

/*
 * If a resource entry has a string name (rather than an ID), go find
 * the string and convert it from Unicode to ASCII.
 */
void get_resource_name_from_id(DWORD id, DWORD resource_base, wchar_t *buffer, UINT cBytes)
{
	IMAGE_RESOURCE_DIR_STRING_U *prdsu;

	// If it's a regular ID, just format it.
	if (!(id & IMAGE_RESOURCE_NAME_IS_STRING)) {
		swprintf(buffer, L"%X", id);
		return;
	}

	id &= 0x7FFFFFFF;
	prdsu = (IMAGE_RESOURCE_DIR_STRING_U *)(resource_base + id);

	/* prdsu->Length is the number of Unicode characters */
	/* add 1 to Length to get the favorite little NULL byte */
	StrcopyW(buffer, prdsu->Length + 1, prdsu->NameString);
}

/*
 * Dump the information about one resource directory entry.  If the
 * entry is for a subdirectory, call the directory dumping routine
 * instead of printing information in this routine.
 */
void dump_resource_entry
(
    IMAGE_RESOURCE_DIRECTORY_ENTRY *resDirEntry,
    DWORD resource_base,
    DWORD level
)
{
	wchar_t nameBuffer[128];

    if (resDirEntry->OffsetToData & IMAGE_RESOURCE_DATA_IS_DIRECTORY) {
        dump_resource_directory((IMAGE_RESOURCE_DIRECTORY *)((resDirEntry->OffsetToData & 0x7FFFFFFF) + resource_base),
								resource_base, level, resDirEntry->Name);
        /* if resource is directory, return since it doesn't have a name to find */
        return;
    }

    if (resDirEntry->Name & IMAGE_RESOURCE_NAME_IS_STRING) {
        get_resource_name_from_id(resDirEntry->Name, resource_base, nameBuffer,
                              sizeof(nameBuffer));
    }
}

/*
 * Dump the information about one resource directory.
 */
void dump_resource_directory
(
    IMAGE_RESOURCE_DIRECTORY *resDir,
    DWORD resource_base,
    DWORD level,
    DWORD resourceType
)
{
    IMAGE_RESOURCE_DIRECTORY_ENTRY *resDirEntry;
    wchar_t szType[64];
    UINT i;

    /* Level 1 resources are the resource types */
    if (level == 1) {
		if (resourceType & IMAGE_RESOURCE_NAME_IS_STRING) {
			get_resource_name_from_id(resourceType, resource_base, szType, sizeof(szType));
		} else {
	        get_resource_type_name( resourceType, szType, sizeof(szType) );
		}
	} else {    /* All other levels, just print out the regular id or name */
        get_resource_name_from_id(resourceType, resource_base, szType, sizeof(szType));
    }

	/*
	 * The "directory entries" immediately follow the directory in memory
	 */
    resDirEntry = (IMAGE_RESOURCE_DIRECTORY_ENTRY *)(resDir+1);

	/* If it's a string table, save off info for future use */
	if (level == 1 && (resourceType == (DWORD)RT_STRING)) {
		pStrResEntries = resDirEntry;
		cStrResEntries = resDir->NumberOfIdEntries + resDir->NumberOfNamedEntries;
	}

	/* If it's a dialog, save off info for future use */
	if (level == 1 && (resourceType == (DWORD)RT_DIALOG)) {
		pDlgResEntries = resDirEntry;
		cDlgResEntries = resDir->NumberOfIdEntries + resDir->NumberOfNamedEntries;
	}

	/* If it's a menu, save off info for future use */
	if (level == 1 && (resourceType == (DWORD)RT_MENU)) {
		pMnuResEntries = resDirEntry;
		cMnuResEntries = resDir->NumberOfIdEntries + resDir->NumberOfNamedEntries;
	}

	/* start iterating through level 1 entries */
    for ( i=0; i < resDir->NumberOfNamedEntries; i++, resDirEntry++ )
        dump_resource_entry(resDirEntry, resource_base, level+1);

	/* iterate through level 2 entries */
    for ( i=0; i < resDir->NumberOfIdEntries; i++, resDirEntry++ )
        dump_resource_entry(resDirEntry, resource_base, level+1);
}

/*
 * Top level routine called to dump out the entire resource hierarchy
 */
void dump_resource_section(DWORD module_base, IMAGE_RESOURCE_DIRECTORY *resDir, push_command_list_s *cmd_list)
{
    dump_resource_directory(resDir, (DWORD)resDir, 0, 0);

    /* dump string table */
	if (cStrResEntries) {
		dump_string_table( 	module_base, (DWORD)resDir,
							pStrResEntries, cStrResEntries, cmd_list);
	}

	/* dump dialogs */
	if (cDlgResEntries) {
		dump_dialogs(	module_base, (DWORD)resDir,
						pDlgResEntries, cDlgResEntries, cmd_list);
	}

	/* dump menus */
	if (cMnuResEntries) {
		dump_menus(	module_base, (DWORD)resDir,
					pMnuResEntries, cMnuResEntries, cmd_list);
	}
}

void get_all_push_cmds(push_command_list_s *cmd_list)
{
	/* prepare local variables */
	DWORD		psize			= 0;
	DWORD		dsize 			= 0;
	DWORD		next_addr		= 0;
	DWORD		start_addr		= current_module->codebase;
	DWORD		current_addr	= start_addr;
	DWORD		end_addr		= current_module->codebase + current_module->codesize;
	DWORD		blocksize		= end_addr - start_addr + 16;	/* give a 16 byte buffer so the last command of the procedure can be read */
	uchar		*decode	= NULL;
	t_reg		*reg	= NULL;
	t_dump		*cpuasm	= NULL;
	uchar		cmdbuf[MAXCMDSIZE]	= { 0 };
	t_disasm	disasm_result;
	t_operand	*cmd_operand;
	push_command_list_s *tmp = NULL;


	cpuasm = Getcpudisasmdump();

	do {
		/* decode current command information and determine command size in bytes */
		current_addr += psize;
		decode = Finddecode(current_addr, &dsize);
		Readmemory(cmdbuf, current_addr, MAXCMDSIZE, MM_SILENT|MM_PARTIAL);
		next_addr = Disassembleforward(NULL, start_addr, blocksize, current_addr, 1, USEDECODE);
		psize = next_addr - current_addr;
		if (psize <= 0) {
			psize = 1;
		}
		reg = Threadregisters(cpuasm->threadid);
		Disasm(cmdbuf, psize, current_addr, decode, &disasm_result, DA_TEXT|DA_OPCOMM|DA_MEMORY, reg, NULL);

		/* save the address of the command into the list if it has the constant we're looking for */
		if ((disasm_result.cmdtype & D_CMDTYPE) == D_PUSH) {
			cmd_operand = disasm_result.op;
			if ((cmd_operand->features & OP_CONST) == OP_CONST) {
				tmp = (push_command_list_s *)Memalloc(sizeof(push_command_list_s), SILENT|ZEROINIT);
				tmp->address = current_addr;
				tmp->immediate = cmd_operand->opconst;
				StrcopyW(tmp->cmd_buf, TEXTLEN, disasm_result.result);
				list_add_tail(&(tmp->list), &(cmd_list->list));
			}
		}
	} while (current_addr < end_addr);
}

void find_resource_refs(void)
{
	t_dump		*dump				= NULL;
	wchar_t		title[SHORTNAME]	= {0};
	int			n					= 0;
	int			i					= 0;
	BYTE		*resource_memory	= NULL;
	push_command_list_s	push_commands;
	push_command_list_s *tmp		= NULL;
	struct list_head *pos, *q;	/* used to keep position while iterating through list in list_for_each() */

	/* clear log table */
	Deletesorteddatarange(&(logtable.sorted), 0x00000000, 0xFFFFFFFF);

	dump = Getcpudisasmdump();
	/* use module based off code in CPU window */
	current_module = Findmodule(dump->base);

	/* set title with name of module that was searched */
	n = StrcopyW(title, SHORTNAME, OLLYRESREFS_NAME);
	n += StrcopyW(title + n, SHORTNAME - n, L" - ");
	n += StrcopyW(title + n, SHORTNAME - n, current_module->modname);
	SetWindowText(logtable.hparent, (LPWSTR)title);

	/* search the code for all "push" commands and save them to a list */
	INIT_LIST_HEAD(&push_commands.list);
	get_all_push_cmds(&push_commands);

	/* allocate and read resource section into memory */
	resource_memory = Memalloc(current_module->ressize, ZEROINIT);
	if (resource_memory == NULL) {
		Addtolist(0, DRAW_HILITE, L"Failed to recreate resource section");
		return;
	}

	Readmemory(resource_memory, current_module->resbase, current_module->ressize, MM_SILENT);

	IMAGE_RESOURCE_DIRECTORY *root_directory = (IMAGE_RESOURCE_DIRECTORY *)resource_memory;

	/* find the resource section rva from the section header */
	for (i = 0; i < current_module->nsect; i++) {
		if (wcscmp(current_module->sect[i].sectname, L".rsrc") == 0) {
			rsrc_sect_base_rva = current_module->sect[i].base;
		}
	}

	dump_resource_section(current_module->base, root_directory, &push_commands);

	if (resource_memory != NULL) {
		Memfree(resource_memory);
	}

	/* free list with push commands */
	list_for_each_safe(pos, q, &push_commands.list) {
		tmp = list_entry(pos, push_command_list_s, list);
		list_del(pos);
		Memfree(tmp);
	}
}

BOOL WINAPI DllMain(HINSTANCE hinstDll, DWORD fdwReason, LPVOID lpReserved)
{
	if (fdwReason == DLL_PROCESS_ATTACH) {
		plugin_inst_g = hinstDll;   /* Save plugin instance */
	}
	return 1;   /* Report success */
};

extc int __cdecl ODBG2_Pluginquery(int iOllyDbgVersion, DWORD *dwFeatures, wchar_t szPluginName[SHORTNAME], wchar_t szPluginVersion[SHORTNAME])
{
	if (iOllyDbgVersion < 201) {
		return 0;
	}
	/* Report name and version to OllyDbg */
	StrcopyW(szPluginName, SHORTNAME, OLLYRESREFS_NAME);
	StrcopyW(szPluginVersion, SHORTNAME, OLLYRESREFS_VERSION);
	return PLUGIN_VERSION;			/* Expected API version */
};

extc int __cdecl ODBG2_Plugininit(void)
{
	if (Createsorteddata(&logtable.sorted, sizeof(LOGDATA), 1, (SORTFUNC *)log_window_sort_proc, NULL, SDM_NOSIZE) == -1) {
		Addtolist(0, DRAW_HILITE, L"%s: Unable to created sorted table data.", OLLYRESREFS_NAME);
		return -1;
	}

	create_log_window();

	Addtolist(0, DRAW_NORMAL, L"[*] %s v%s by Austyn Krutsinger <akrutsinger@gmail.com>", OLLYRESREFS_NAME, OLLYRESREFS_VERSION);

	/* Report success. */
	return 0;
};

extc t_menu * __cdecl ODBG2_Pluginmenu(wchar_t *type)
{
	if (wcscmp(type, PWM_MAIN) == 0) {
		/* Main menu. */
		return plugin_main_menu;
	} else if (wcscmp(type, PWM_DISASM) == 0) {
		/* Disassembler pane of CPU window. */
		return plugin_popup_menu;
	}
	return NULL;                /* No menu */
};

extc void __cdecl ODBG2_Pluginreset(void)
{
	Deletesorteddatarange(&(logtable.sorted), 0x00000000, 0xFFFFFFFF);
}

extc void __cdecl ODBG2_Plugindestroy(void)
{
	Destroysorteddata(&(logtable.sorted));
}
