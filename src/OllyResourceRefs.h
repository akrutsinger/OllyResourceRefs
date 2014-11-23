/*******************************************************************************
 * OllyResourceRefs - OllyResourceRefs.h
 *
 * By Austyn Krutsinger
 *
 ******************************************************************************/

#ifndef __OLLYRESOURCEREFS_H__
#define __OLLYRESOURCEREFS_H__

#define WIN32_LEAN_AND_MEAN
#include <Windows.h>
#include "plugin.h"

#include "list.h"

/*  To use this exported function of dll, include this header
 *  in the project.
 */
#ifdef BUILD_DLL
    #define DLL_EXPORT __declspec(dllexport)
#else
    #define DLL_EXPORT __declspec(dllimport)
#endif

#define OLLYRESREFS_NAME		L"OllyResourceRefs"		/* Unique plugin name */
#define OLLYRESREFS_VERSION		L"0.1.0"				/* Plugin version (stable . update . patch  - status) */

#ifdef __cplusplus
extern "C" {
#endif

/* Menu items */
#define MENU_FIND_RES_REFS		1
#define	MENU_ABOUT				2
#define MENU_VIEW_IN_CPU		3

/* Global Declarations */


/**
 * Forward declarations
 */
 /* Menu functions */
int menu_handler(t_table* pTable, wchar_t* pszName, DWORD dwIndex, int iMode);
void about_message(void);

/* Log window functions */
void create_log_window(void);
long log_window_proc(t_table *pTable, HWND hWnd, UINT uMsg, WPARAM wParam, LPARAM lParam);
long log_window_draw(wchar_t *szBuffer, uchar *pMask, int *pSelect, t_table *pTable, t_drawheader *pHeader, int iColumn, void *pCache);
int log_window_sort_proc(const t_sorthdr *pSortHeader1, const t_sorthdr *pSortHeader2, const int iSort);

void dump_resource_directory(IMAGE_RESOURCE_DIRECTORY *resDir, DWORD resource_base, DWORD level, DWORD resourceType);

void find_resource_refs(void);

typedef struct _LOGDATA {
	/* Obligatory header, its layout _must_ coincide with t_sorthdr! */
	DWORD	address;	/* address of the call; NOTE: set SDM_NOSIZE flag in Createsorteddata() */

	/* Custom data follows header. */
	wchar_t	command[TEXTLEN];       /* command that references resource */
	wchar_t	item_text[TEXTLEN];     /* text of string, menu, dialog item, etc */
	wchar_t	resource_type[TEXTLEN]; /* type of resource: menu, icon, string, etc + ID */
} LOGDATA, *LPLOGDATA;

typedef struct _push_command_list_s {
	DWORD address;      /* address of push command */
	DWORD immediate;    /* immediate valued of the command */
	wchar_t cmd_buf[TEXTLEN];
	struct list_head list;
} push_command_list_s;


#ifdef __cplusplus
}
#endif

#endif	/* __OLLYRESOURCEREFS_H__ */
