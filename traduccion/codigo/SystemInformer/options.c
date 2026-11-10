/*
 * Copyright (c) 2022 Winsider Seminars & Solutions, Inc.  All rights reserved.
 *
 * This file is part of System Informer.
 *
 * Authors:
 *
 *     wj32    2010-2016
 *     dmex    2017-2026
 *
 */

#include <phapp.h>

#include <commdlg.h>
#include <colmgr.h>
#include <colorbox.h>
#include <cpysave.h>
#include <settings.h>
#include <emenu.h>
#include <json.h>

#include <mainwnd.h>
#include <mainwndp.h>
#include <notifico.h>
#include <proctree.h>
#include <phplug.h>
#include <phsettings.h>

#define WM_PH_CHILD_EXIT (WM_APP + 301)

INT_PTR CALLBACK PhpOptionsGeneralDlgProc(
    _In_ HWND hwndDlg,
    _In_ UINT uMsg,
    _In_ WPARAM wParam,
    _In_ LPARAM lParam
    );

INT_PTR CALLBACK PhpOptionsAdvancedDlgProc(
    _In_ HWND hwndDlg,
    _In_ UINT uMsg,
    _In_ WPARAM wParam,
    _In_ LPARAM lParam
    );

INT_PTR CALLBACK PhpOptionsHighlightingDlgProc(
    _In_ HWND hwndDlg,
    _In_ UINT uMsg,
    _In_ WPARAM wParam,
    _In_ LPARAM lParam
    );

INT_PTR CALLBACK PhpOptionsTrayIconDlgProc(
    _In_ HWND hwndDlg,
    _In_ UINT uMsg,
    _In_ WPARAM wParam,
    _In_ LPARAM lParam
    );

INT_PTR CALLBACK PhpOptionsThemesDlgProc(
    _In_ HWND hwndDlg,
    _In_ UINT uMsg,
    _In_ WPARAM wParam,
    _In_ LPARAM lParam
    );

INT_PTR CALLBACK PhpOptionsGraphsDlgProc(
    _In_ HWND hwndDlg,
    _In_ UINT uMsg,
    _In_ WPARAM wParam,
    _In_ LPARAM lParam
    );

INT_PTR CALLBACK PhOptionsDialogProc(
    _In_ HWND hwndDlg,
    _In_ UINT uMsg,
    _In_ WPARAM wParam,
    _In_ LPARAM lParam
    );

VOID PhOptionsDestroySection(
    _In_ PPH_OPTIONS_SECTION Section
    );

_Function_class_(PH_OPTIONS_ENTER_SECTION_VIEW)
VOID PhOptionsEnterSectionView(
    _In_ PPH_OPTIONS_SECTION NewSection
    );

VOID PhOptionsLayoutSectionView(
    VOID
    );

VOID PhOptionsEnterSectionViewInner(
    _In_ PPH_OPTIONS_SECTION Section,
    _Inout_ HDWP *ContainerDeferHandle
    );

VOID PhOptionsCreateSectionDialog(
    _In_ PPH_OPTIONS_SECTION Section
    );

_Function_class_(PH_OPTIONS_FIND_SECTION)
PPH_OPTIONS_SECTION PhOptionsFindSection(
    _In_ PCPH_STRINGREF Name
    );

VOID PhOptionsOnSize(
    VOID
    );

_Function_class_(PH_OPTIONS_CREATE_SECTION)
PPH_OPTIONS_SECTION PhOptionsCreateSection(
    _In_ PCWSTR Name,
    _In_ PVOID Instance,
    _In_ PCWSTR Template,
    _In_ DLGPROC DialogProc,
    _In_opt_ PVOID Parameter
    );

PPH_OPTIONS_SECTION PhOptionsCreateSectionAdvanced(
    _In_ PCWSTR Name,
    _In_ PVOID Instance,
    _In_ PCWSTR Template,
    _In_ DLGPROC DialogProc,
    _In_opt_ PVOID Parameter
    );

BOOLEAN PhpIsDefaultTaskManager(
    VOID
    );

VOID PhpSetDefaultTaskManager(
    _In_ HWND ParentWindowHandle
    );

static HWND PhOptionsWindowHandle = NULL;
static PH_LAYOUT_MANAGER WindowLayoutManager;

static PPH_LIST SectionList = NULL;
static PPH_OPTIONS_SECTION CurrentSection = NULL;
static HWND OptionsTreeControl = NULL;
static HWND ContainerControl = NULL;
static RECT MinimumSize;
static PCWSTR OptionsInitialSectionName = NULL;

// All
static BOOLEAN RestartRequired = FALSE;

// General
static BOOLEAN GeneralListViewStateInitializing = FALSE;
static BOOLEAN ThemeListViewStateInitializing = FALSE;
static CONST PH_STRINGREF CurrentUserRunKeyName = PH_STRINGREF_INIT(L"Software\\Microsoft\\Windows\\CurrentVersion\\Run");
static BOOLEAN CurrentUserRunPresent = FALSE;
static HFONT CurrentFontInstance = NULL;
static HFONT CurrentFontMonospaceInstance = NULL;
static PPH_STRING NewFontSelection = NULL;
static PPH_STRING NewFontMonospaceSelection = NULL;

// Advanced
static CONST PH_STRINGREF TaskMgrImageOptionsKeyName = PH_STRINGREF_INIT(L"Software\\Microsoft\\Windows NT\\CurrentVersion\\Image File Execution Options\\taskmgr.exe");
static PPH_STRING OldTaskMgrDebugger = NULL;
static HWND WindowHandleForElevate = NULL;

// Highlighting
static HWND HighlightingListViewHandle = NULL;

VOID PhShowOptionsDialog(
    _In_ HWND ParentWindowHandle,
    _In_opt_ PCWSTR SectionName
    )
{
    if (PhStartupParameters.ShowOptions)
    {
        PhpSetDefaultTaskManager(ParentWindowHandle);
    }
    else
    {
        if (!PhOptionsWindowHandle)
        {
            OptionsInitialSectionName = SectionName;
            PhOptionsWindowHandle = PhCreateDialog(
                PhInstanceHandle,
                MAKEINTRESOURCE(IDD_OPTIONS),
                NULL,
                PhOptionsDialogProc,
                NULL
                );
            OptionsInitialSectionName = NULL;

            PhRegisterDialog(PhOptionsWindowHandle);
            ShowWindow(PhOptionsWindowHandle, SW_SHOW);
        }
        else if (SectionName)
        {
            PH_STRINGREF sectionNameSr;
            PPH_OPTIONS_SECTION section;

            PhInitializeStringRef(&sectionNameSr, SectionName);

            if (section = PhOptionsFindSection(&sectionNameSr))
                TreeView_SelectItem(OptionsTreeControl, section->TreeItemHandle);
        }

        if (IsMinimized(PhOptionsWindowHandle))
            ShowWindow(PhOptionsWindowHandle, SW_RESTORE);
        else
            SetForegroundWindow(PhOptionsWindowHandle);
    }
}

static HTREEITEM PhpTreeViewInsertItem(
    _In_opt_ HTREEITEM HandleInsertAfter,
    _In_ PCWSTR Text,
    _In_ PVOID Context
    )
{
    TV_INSERTSTRUCT insert;

    memset(&insert, 0, sizeof(TV_INSERTSTRUCT));

    insert.hParent = TVI_ROOT;
    insert.hInsertAfter = HandleInsertAfter;
    insert.item.mask = TVIF_TEXT | TVIF_PARAM;
    insert.item.pszText = (PWSTR)Text;
    insert.item.lParam = (LPARAM)Context;

    return TreeView_InsertItem(OptionsTreeControl, &insert);
}

static VOID PhpOptionsShowHideTreeViewItem(
    _In_ BOOLEAN Hide
    )
{
    static CONST PH_STRINGREF generalName = PH_STRINGREF_INIT(L"General");
    static CONST PH_STRINGREF advancedName = PH_STRINGREF_INIT(L"Avanzado");

    if (Hide)
    {
        PPH_OPTIONS_SECTION advancedSection;

        if (advancedSection = PhOptionsFindSection(&advancedName))
        {
            if (advancedSection->TreeItemHandle)
            {
                TreeView_DeleteItem(OptionsTreeControl, advancedSection->TreeItemHandle);
                advancedSection->TreeItemHandle = NULL;
            }
        }
    }
    else
    {
        PPH_OPTIONS_SECTION generalSection;
        PPH_OPTIONS_SECTION advancedSection;

        generalSection = PhOptionsFindSection(&generalName);
        advancedSection = PhOptionsFindSection(&advancedName);

        if (generalSection && advancedSection && !advancedSection->TreeItemHandle)
        {
            advancedSection->TreeItemHandle = PhpTreeViewInsertItem(
                generalSection->TreeItemHandle,
                advancedName.Buffer,
                advancedSection
                );
        }
    }
}

VOID PhpAdvancedPageSave(
    _In_ HWND hwndDlg
    );

static VOID PhpOptionsNotifyChangeCallback(
    _In_ PVOID Context
    );

VOID PhpAdvancedPageLoad(
    _In_ HWND hwndDlg,
    _In_ BOOLEAN ReloadOnly
    );

static VOID PhReloadGeneralSection(
    VOID
    )
{
    static PH_STRINGREF generalName = PH_STRINGREF_INIT(L"General");

    GeneralListViewStateInitializing = TRUE;
    PhpAdvancedPageLoad(PhOptionsFindSection(&generalName)->DialogHandle, TRUE);
    GeneralListViewStateInitializing = FALSE;
}

static VOID PhReloadThemesSection(
    VOID
    )
{
    static PH_STRINGREF themesName = PH_STRINGREF_INIT(L"Temas");
    PPH_OPTIONS_SECTION section;

    section = PhOptionsFindSection(&themesName);
    if (!section || !section->DialogHandle)
        return;

    ThemeListViewStateInitializing = TRUE;
    Button_SetCheck(GetDlgItem(section->DialogHandle, IDC_ENABLETHEME), PhGetIntegerSetting(SETTING_ENABLE_THEME_SUPPORT) ? BST_CHECKED : BST_UNCHECKED);
    ComboBox_SetCurSel(GetDlgItem(section->DialogHandle, IDC_THEMEMODE), PhGetIntegerSetting(SETTING_THEME_MODE));
    EnableWindow(GetDlgItem(section->DialogHandle, IDC_THEMEMODE), PhGetIntegerSetting(SETTING_ENABLE_THEME_SUPPORT) != 0);
    ThemeListViewStateInitializing = FALSE;
}

static VOID PhpApplyThemeSupportSetting(
    _In_ HWND hwndDlg
    )
{
    BOOLEAN enableThemeSupport;
    BOOLEAN themeSupportWasEnabled;

    themeSupportWasEnabled = !!PhGetIntegerSetting(SETTING_ENABLE_THEME_SUPPORT);
    enableThemeSupport = Button_GetCheck(GetDlgItem(hwndDlg, IDC_ENABLETHEME)) == BST_CHECKED;
    PhSetIntegerSetting(SETTING_ENABLE_THEME_SUPPORT, enableThemeSupport);

    // Enabling theme support forces dark graph backgrounds (GraphColorMode); undo that
    // override when theme support is turned off so the graphs return to the light colors.
    if (themeSupportWasEnabled && !enableThemeSupport)
        PhSetIntegerSetting(SETTING_GRAPH_COLOR_MODE, 0);

    PhUpdateCachedSettings();

    // The General page also lists this setting and saves its check state when the options
    // dialog closes, so reload it or the stale state would revert the change made here.
    PhReloadGeneralSection();

    // The theme mode combo is only meaningful while theme support is enabled.
    EnableWindow(GetDlgItem(hwndDlg, IDC_THEMEMODE), enableThemeSupport);

    // Control superclasses (statusbar, menus, dialogs) are only registered during
    // startup while theme support is enabled, so the toggle cannot be applied to
    // windows that already exist. PhEnableThemeSupport keeps the startup state and
    // the new setting takes effect after a restart.
    if (enableThemeSupport != PhEnableThemeSupport)
        PhShowOptionsRestartRequired(hwndDlg);
}

static VOID PhpApplyThemeModeSetting(
    _In_ HWND hwndDlg
    )
{
    INT mode;

    mode = ComboBox_GetCurSel(GetDlgItem(hwndDlg, IDC_THEMEMODE));

    if (mode == CB_ERR)
        return;

    PhSetIntegerSetting(SETTING_THEME_MODE, mode);

    // Graph drawing keys off GraphColorMode rather than the window palette;
    // pair a dark palette with dark graph backgrounds (same override the
    // advanced options page applies).
    if (PhEnableThemeSupport && PhIsThemeModeDark(mode))
        PhSetIntegerSetting(SETTING_GRAPH_COLOR_MODE, 1);

    PhUpdateCachedSettings();

    if (PhEnableThemeSupport)
    {
        PhApplyThemeModeWithColors(mode, PhMainWndHandle);
        PhMwpInitializeMainMenu(PhMainWndHandle);
    }
}

static VOID PhpOptionsSetImageList(
    _In_ HWND WindowHandle,
    _In_ BOOLEAN Treeview
    )
{
    HIMAGELIST imageListHandle;
    LONG dpiValue = PhGetWindowDpi(WindowHandle);

    if (Treeview)
        imageListHandle = TreeView_GetImageList(WindowHandle, TVSIL_NORMAL);
    else
        imageListHandle = ListView_GetImageList(WindowHandle, LVSIL_SMALL);

    if (imageListHandle)
    {
        PhImageListSetIconSize(imageListHandle, 2, PhScaleToDisplay(24, dpiValue));

        if (Treeview)
            TreeView_SetImageList(WindowHandle, imageListHandle, TVSIL_NORMAL);
        else
            ListView_SetImageList(WindowHandle, imageListHandle, LVSIL_SMALL);
    }
    else
    {
        if (imageListHandle = PhImageListCreate(2, PhScaleToDisplay(24, dpiValue), ILC_MASK | ILC_COLOR32, 1, 1))
        {
            if (Treeview)
                TreeView_SetImageList(WindowHandle, imageListHandle, TVSIL_NORMAL);
            else
                ListView_SetImageList(WindowHandle, imageListHandle, LVSIL_SMALL);
        }
    }
}

INT_PTR CALLBACK PhOptionsDialogProc(
    _In_ HWND hwndDlg,
    _In_ UINT uMsg,
    _In_ WPARAM wParam,
    _In_ LPARAM lParam
    )
{
    switch (uMsg)
    {
    case WM_INITDIALOG:
        {
            OptionsTreeControl = GetDlgItem(hwndDlg, IDC_SECTIONTREE);
            ContainerControl = GetDlgItem(hwndDlg, IDD_CONTAINER);

            PhSetApplicationWindowIcon(hwndDlg);

            PhpOptionsSetImageList(OptionsTreeControl, TRUE);

            //PhSetWindowStyle(GetDlgItem(hwndDlg, IDC_SEPARATOR), SS_OWNERDRAW, SS_OWNERDRAW);
            PhSetControlTheme(OptionsTreeControl, L"explorer");
            TreeView_SetExtendedStyle(OptionsTreeControl, TVS_EX_DOUBLEBUFFER, TVS_EX_DOUBLEBUFFER);
            TreeView_SetBkColor(OptionsTreeControl, GetSysColor(COLOR_3DFACE));

            MinimumSize.left = 0;
            MinimumSize.top = 0;
            MinimumSize.right = 423;
            MinimumSize.bottom = 247;
            MapDialogRect(hwndDlg, &MinimumSize);

            PhInitializeLayoutManager(&WindowLayoutManager, hwndDlg);
            PhAddLayoutItem(&WindowLayoutManager, OptionsTreeControl, NULL, PH_ANCHOR_LEFT | PH_ANCHOR_TOP | PH_ANCHOR_BOTTOM);
            //PhAddLayoutItem(&WindowLayoutManager, GetDlgItem(hwndDlg, IDC_SEPARATOR), NULL, PH_ANCHOR_LEFT | PH_ANCHOR_TOP | PH_ANCHOR_BOTTOM);
            PhAddLayoutItem(&WindowLayoutManager, ContainerControl, NULL, PH_ANCHOR_LEFT | PH_ANCHOR_TOP | PH_ANCHOR_RIGHT | PH_ANCHOR_BOTTOM);
            PhAddLayoutItem(&WindowLayoutManager, GetDlgItem(hwndDlg, IDC_RESET), NULL, PH_ANCHOR_LEFT | PH_ANCHOR_BOTTOM);
            PhAddLayoutItem(&WindowLayoutManager, GetDlgItem(hwndDlg, IDC_CLEANUP), NULL, PH_ANCHOR_LEFT | PH_ANCHOR_BOTTOM);
            //PhAddLayoutItem(&WindowLayoutManager, GetDlgItem(hwndDlg, IDC_APPLY), NULL, PH_ANCHOR_RIGHT | PH_ANCHOR_BOTTOM);
            PhAddLayoutItem(&WindowLayoutManager, GetDlgItem(hwndDlg, IDOK), NULL, PH_ANCHOR_RIGHT | PH_ANCHOR_BOTTOM);

            PhRegisterWindowCallback(hwndDlg, PH_PLUGIN_WINDOW_EVENT_TYPE_TOPMOST, NULL);

            if (PhEnableThemeSupport) // TODO: fix options dialog theme (dmex)
                PhInitializeWindowTheme(hwndDlg, TRUE);

            {
                PPH_OPTIONS_SECTION section;

                SectionList = PhCreateList(8);
                CurrentSection = NULL;

                section = PhOptionsCreateSection(L"General", PhInstanceHandle, MAKEINTRESOURCE(IDD_OPTGENERAL), PhpOptionsGeneralDlgProc, NULL);
                PhOptionsCreateSectionAdvanced(L"Avanzado", PhInstanceHandle, MAKEINTRESOURCE(IDD_OPTADVANCED), PhpOptionsAdvancedDlgProc, NULL);
                PhOptionsCreateSection(L"Resaltado", PhInstanceHandle, MAKEINTRESOURCE(IDD_OPTHIGHLIGHTING), PhpOptionsHighlightingDlgProc, NULL);
                PhOptionsCreateSection(L"Icono de la bandeja", PhInstanceHandle, MAKEINTRESOURCE(IDD_OPTTRAYICON), PhpOptionsTrayIconDlgProc, NULL);
                PhOptionsCreateSection(L"Gráficos", PhInstanceHandle, MAKEINTRESOURCE(IDD_OPTGRAPHS), PhpOptionsGraphsDlgProc, NULL);
                PhOptionsCreateSection(L"Complementos", PhInstanceHandle, MAKEINTRESOURCE(IDD_PLUGINS), PhPluginsDlgProc, NULL);

                if (PhPluginsEnabled)
                {
                    PH_PLUGIN_OPTIONS_POINTERS pointers;

                    pointers.WindowHandle = hwndDlg;
                    pointers.CreateSection = PhOptionsCreateSection;
                    pointers.FindSection = PhOptionsFindSection;
                    pointers.EnterSectionView = PhOptionsEnterSectionView;

                    PhInvokeCallback(PhGetGeneralCallback(GeneralCallbackOptionsWindowInitializing), &pointers);
                }

                if (OptionsInitialSectionName)
                {
                    PH_STRINGREF sectionNameSr;
                    PPH_OPTIONS_SECTION initialSection;

                    PhInitializeStringRef(&sectionNameSr, OptionsInitialSectionName);

                    if (initialSection = PhOptionsFindSection(&sectionNameSr))
                        section = initialSection;
                }

                TreeView_SelectItem(OptionsTreeControl, section->TreeItemHandle);
                PhSetDialogFocus(hwndDlg, OptionsTreeControl);
                //PhOptionsEnterSectionView(section);
                PhOptionsOnSize();
            }

            if (PhValidWindowPlacementFromSetting(SETTING_OPTIONS_WINDOW_POSITION))
                PhLoadWindowPlacementFromSetting(SETTING_OPTIONS_WINDOW_POSITION, SETTING_OPTIONS_WINDOW_SIZE, hwndDlg);
            else
                PhCenterWindow(hwndDlg, PhMainWndHandle);
        }
        break;
    case WM_DESTROY:
        {
            ULONG i;
            PPH_OPTIONS_SECTION section;

            PhSaveWindowPlacementToSetting(SETTING_OPTIONS_WINDOW_POSITION, SETTING_OPTIONS_WINDOW_SIZE, hwndDlg);

            PhDeleteLayoutManager(&WindowLayoutManager);

            for (i = 0; i < SectionList->Count; i++)
            {
                section = SectionList->Items[i];

            if (PhEqualStringRef2(&section->Name, L"General", TRUE))
            {
                PhpAdvancedPageSave(section->DialogHandle);
            }

            PhOptionsDestroySection(section);
        }

            SystemInformer_Invoke(PhpOptionsNotifyChangeCallback, NULL);

            PhDereferenceObject(SectionList);
            SectionList = NULL;

            PhUnregisterWindowCallback(hwndDlg);

            PhUnregisterDialog(PhOptionsWindowHandle);
            PhOptionsWindowHandle = NULL;
        }
        break;
    case WM_DPICHANGED:
        {
            PhpOptionsSetImageList(OptionsTreeControl, TRUE);
            PhLayoutManagerUpdate(&WindowLayoutManager, LOWORD(wParam));
            PhOptionsOnSize();
        }
        break;
    case WM_SIZE:
        {
            PhOptionsOnSize();
        }
        break;
    case WM_SIZING:
        {
            PhResizingMinimumSize((PRECT)lParam, wParam, MinimumSize.right, MinimumSize.bottom);
        }
        break;
    case WM_COMMAND:
        {
            switch (GET_WM_COMMAND_ID(wParam, lParam))
            {
            case IDCANCEL:
            case IDOK:
                {
                    DestroyWindow(hwndDlg);
                }
                break;
            case IDC_RESET:
                {
                    if (PhShowMessage2(
                        hwndDlg,
                        TD_YES_BUTTON | TD_NO_BUTTON,
                        TD_WARNING_ICON,
                        L"¿Desea restablecer todos los ajustes y reiniciar System Informer?",
                        L""
                        ) == IDYES)
                    {
                        SystemInformer_PrepareForEarlyShutdown();

                        PhResetSettings(PhMainWndHandle);
                        PhSaveSettings2(PhSettingsFileName);

                        if (NT_SUCCESS(PhShellProcessHacker(
                            PhMainWndHandle,
                            L"-v -newinstance",
                            SW_SHOW,
                            PH_SHELL_EXECUTE_DEFAULT,
                            PH_SHELL_APP_PROPAGATE_PARAMETERS | PH_SHELL_APP_PROPAGATE_PARAMETERS_IGNORE_VISIBILITY,
                            0,
                            NULL
                            )))
                        {
                            SystemInformer_Destroy();
                        }
                        else
                        {
                            SystemInformer_CancelEarlyShutdown();
                        }
                    }
                }
                break;
            case IDC_CLEANUP:
                {
                    if (PhShowMessage2(
                        hwndDlg,
                        TD_YES_BUTTON | TD_NO_BUTTON,
                        TD_INFORMATION_ICON,
                        L"¿Desea limpiar los ajustes no utilizados?",
                        L""
                        ) == IDYES)
                    {
                        PhClearIgnoredSettings();
                    }
                }
                break;
            }
        }
        break;
    case WM_DRAWITEM:
        {
            PDRAWITEMSTRUCT drawInfo = (PDRAWITEMSTRUCT)lParam;

            //if (drawInfo->CtlID == IDC_SEPARATOR)
            //{
            //    RECT rect;
            //
            //    rect = drawInfo->rcItem;
            //    rect.right = 2;
            //
            //    if (PhEnableThemeSupport)
            //    {
            //        switch (PhCsGraphColorMode)
            //        {
            //        case 0: // New colors
            //            {
            //                FillRect(drawInfo->hDC, &rect, GetSysColorBrush(COLOR_3DHIGHLIGHT));
            //                rect.left += 1;
            //                FillRect(drawInfo->hDC, &rect, GetSysColorBrush(COLOR_3DSHADOW));
            //            }
            //            break;
            //        case 1: // Old colors
            //            {
            //                SetDCBrushColor(drawInfo->hDC, RGB(0, 0, 0));
            //                FillRect(drawInfo->hDC, &rect, PhGetStockBrush(DC_BRUSH));
            //            }
            //            break;
            //        }
            //    }
            //    else
            //    {
            //        FillRect(drawInfo->hDC, &rect, GetSysColorBrush(COLOR_3DHIGHLIGHT));
            //        rect.left += 1;
            //        FillRect(drawInfo->hDC, &rect, GetSysColorBrush(COLOR_3DSHADOW));
            //    }
            //
            //    return TRUE;
            //}
        }
        break;
    case WM_NOTIFY:
        {
            LPNMHDR header = (LPNMHDR)lParam;

            switch (header->code)
            {
            case TVN_SELCHANGED:
                {
                    LPNMTREEVIEW treeview = (LPNMTREEVIEW)lParam;
                    PPH_OPTIONS_SECTION section = (PPH_OPTIONS_SECTION)treeview->itemNew.lParam;

                    if (section)
                    {
                        PhOptionsEnterSectionView(section);
                    }
                }
                break;
            case NM_SETCURSOR:
                {
                    if (header->hwndFrom == OptionsTreeControl)
                    {
                        PhSetCursor(PhLoadArrowCursor());

                        SetWindowLongPtr(hwndDlg, DWLP_MSGRESULT, TRUE);
                        return TRUE;
                    }
                }
                break;
            }
        }
        break;
    }

    return FALSE;
}

VOID PhOptionsOnSize(
    VOID
    )
{
    PhLayoutManagerLayout(&WindowLayoutManager);

    if (SectionList && SectionList->Count != 0)
    {
        PhOptionsLayoutSectionView();
    }
}

_Function_class_(PH_OPTIONS_CREATE_SECTION)
PPH_OPTIONS_SECTION PhOptionsCreateSection(
    _In_ PCWSTR Name,
    _In_ PVOID Instance,
    _In_ PCWSTR Template,
    _In_ DLGPROC DialogProc,
    _In_opt_ PVOID Parameter
    )
{
    PPH_OPTIONS_SECTION section;

    section = PhAllocateZero(sizeof(PH_OPTIONS_SECTION));
    PhInitializeStringRefLongHint(&section->Name, Name);
    section->Instance = Instance;
    section->Template = Template;
    section->DialogProc = DialogProc;
    section->Parameter = Parameter;
    section->TreeItemHandle = PhpTreeViewInsertItem(TVI_LAST, Name, section);

    PhAddItemList(SectionList, section);

    return section;
}

PPH_OPTIONS_SECTION PhOptionsCreateSectionAdvanced(
    _In_ PCWSTR Name,
    _In_ PVOID Instance,
    _In_ PCWSTR Template,
    _In_ DLGPROC DialogProc,
    _In_opt_ PVOID Parameter
    )
{
    PPH_OPTIONS_SECTION section;

    section = PhAllocateZero(sizeof(PH_OPTIONS_SECTION));
    PhInitializeStringRefLongHint(&section->Name, Name);
    section->Instance = Instance;
    section->Template = Template;
    section->DialogProc = DialogProc;
    section->Parameter = Parameter;

    PhAddItemList(SectionList, section);

    return section;
}

VOID PhOptionsDestroySection(
    _In_ PPH_OPTIONS_SECTION Section
    )
{
    PhFree(Section);
}

_Function_class_(PH_OPTIONS_FIND_SECTION)
PPH_OPTIONS_SECTION PhOptionsFindSection(
    _In_ PCPH_STRINGREF Name
    )
{
    ULONG i;
    PPH_OPTIONS_SECTION section;

    for (i = 0; i < SectionList->Count; i++)
    {
        section = SectionList->Items[i];

        if (PhEqualStringRef(&section->Name, Name, TRUE))
            return section;
    }

    return NULL;
}

VOID PhOptionsLayoutSectionView(
    VOID
    )
{
    if (CurrentSection && CurrentSection->DialogHandle)
    {
        RECT clientRect;

        if (!PhGetClientRect(ContainerControl, &clientRect))
            return;

        SetWindowPos(
            CurrentSection->DialogHandle,
            NULL,
            0,
            0,
            clientRect.right - clientRect.left,
            clientRect.bottom - clientRect.top,
            SWP_NOACTIVATE | SWP_NOZORDER
            );
    }
}

_Function_class_(PH_OPTIONS_ENTER_SECTION_VIEW)
VOID PhOptionsEnterSectionView(
    _In_ PPH_OPTIONS_SECTION NewSection
    )
{
    ULONG i;
    PPH_OPTIONS_SECTION section;
    PPH_OPTIONS_SECTION oldSection;
    HDWP containerDeferHandle;

    if (CurrentSection == NewSection)
        return;

    oldSection = CurrentSection;
    CurrentSection = NewSection;

    containerDeferHandle = BeginDeferWindowPos(SectionList->Count);

    PhOptionsEnterSectionViewInner(NewSection, &containerDeferHandle);
    PhOptionsLayoutSectionView();

    for (i = 0; i < SectionList->Count; i++)
    {
        section = SectionList->Items[i];

        if (section != NewSection)
            PhOptionsEnterSectionViewInner(section, &containerDeferHandle);
    }

    EndDeferWindowPos(containerDeferHandle);

    if (NewSection->DialogHandle)
        RedrawWindow(NewSection->DialogHandle, NULL, NULL, RDW_ERASE | RDW_INVALIDATE | RDW_ALLCHILDREN | RDW_UPDATENOW);
}

VOID PhOptionsEnterSectionViewInner(
    _In_ PPH_OPTIONS_SECTION Section,
    _Inout_ HDWP *ContainerDeferHandle
    )
{
    if (Section == CurrentSection && !Section->DialogHandle)
        PhOptionsCreateSectionDialog(Section);

    if (Section->DialogHandle)
    {
        if (Section == CurrentSection)
            *ContainerDeferHandle = DeferWindowPos(*ContainerDeferHandle, Section->DialogHandle, NULL, 0, 0, 0, 0, SWP_SHOWWINDOW_ONLY | SWP_NOREDRAW);
        else
            *ContainerDeferHandle = DeferWindowPos(*ContainerDeferHandle, Section->DialogHandle, NULL, 0, 0, 0, 0, SWP_HIDEWINDOW_ONLY | SWP_NOREDRAW);
    }
}

VOID PhOptionsCreateSectionDialog(
    _In_ PPH_OPTIONS_SECTION Section
    )
{
    Section->DialogHandle = PhCreateDialogFromTemplate(
        ContainerControl,
        DS_SETFONT | DS_FIXEDSYS | DS_CONTROL | WS_CHILD,
        Section->Instance,
        Section->Template,
        Section->DialogProc,
        Section->Parameter
        );

    PhInitializeWindowTheme(Section->DialogHandle, PhEnableThemeSupport);
}

#define SetDlgItemCheckForSetting(hwndDlg, Id, Name) \
    Button_SetCheck(GetDlgItem(hwndDlg, Id), PhGetIntegerSetting(Name) ? BST_CHECKED : BST_UNCHECKED)
#define SetSettingForDlgItemCheck(hwndDlg, Id, Name) \
    PhSetIntegerSetting(Name, Button_GetCheck(GetDlgItem(hwndDlg, Id)) == BST_CHECKED)
#define SetSettingForDlgItemCheckRestartRequired(hwndDlg, Id, Name) \
{ \
    BOOLEAN __oldValue = !!PhGetIntegerSetting(Name); \
    BOOLEAN __newValue = Button_GetCheck(GetDlgItem(hwndDlg, Id)) == BST_CHECKED; \
    if (__newValue != __oldValue) \
        RestartRequired = TRUE; \
    PhSetIntegerSetting(Name, __newValue); \
}

#define SetLvItemCheckForSetting(ListViewHandle, Index, Name) \
    ListView_SetCheckState(ListViewHandle, Index, !!PhGetIntegerSetting(Name));
#define SetSettingForLvItemCheck(ListViewHandle, Index, Name) \
    PhSetIntegerSetting(Name, ListView_GetCheckState(ListViewHandle, Index) == BST_CHECKED)
#define SetSettingForLvItemCheckRestartRequired(ListViewHandle, Index, Name) \
{ \
    BOOLEAN __oldValue = !!PhGetIntegerSetting(Name); \
    BOOLEAN __newValue = ListView_GetCheckState(ListViewHandle, Index) == BST_CHECKED; \
    if (__newValue != __oldValue) \
        RestartRequired = TRUE; \
    PhSetIntegerSetting(Name, __newValue); \
}

_Success_(return)
static BOOLEAN GetCurrentFont(
    _Out_ PLOGFONT Font
    )
{
    BOOLEAN result;
    PPH_STRING fontHexString;

    if (NewFontSelection)
        fontHexString = NewFontSelection;
    else
        fontHexString = PhaGetStringSetting(SETTING_FONT);

    if (fontHexString->Length / sizeof(WCHAR) / 2 == sizeof(LOGFONT))
        result = PhHexStringToBuffer(&fontHexString->sr, (PUCHAR)Font);
    else
        result = FALSE;

    return result;
}

_Success_(return)
static BOOLEAN GetCurrentFontMonospace(
    _Out_ PLOGFONT Font
    )
{
    BOOLEAN result;
    PPH_STRING fontHexString;

    if (NewFontMonospaceSelection)
        fontHexString = NewFontMonospaceSelection;
    else
        fontHexString = PhaGetStringSetting(SETTING_FONT_MONOSPACE);

    if (fontHexString->Length / sizeof(WCHAR) / 2 == sizeof(LOGFONT))
        result = PhHexStringToBuffer(&fontHexString->sr, (PUCHAR)Font);
    else
        result = FALSE;

    return result;
}

typedef struct _PHP_HKURUN_ENTRY
{
    PPH_STRING Value;
    //PPH_STRING Name;
} PHP_HKURUN_ENTRY, *PPHP_HKURUN_ENTRY;

_Function_class_(PH_ENUM_KEY_CALLBACK)
BOOLEAN NTAPI PhpReadCurrentRunCallback(
    _In_ HANDLE RootDirectory,
    _In_ PKEY_VALUE_FULL_INFORMATION Information,
    _In_opt_ PVOID Context
    )
{
    if (Context && Information->Type == REG_SZ)
    {
        PHP_HKURUN_ENTRY entry;

        if (Information->DataLength > sizeof(UNICODE_NULL))
            entry.Value = PhCreateStringEx(PTR_ADD_OFFSET(Information, Information->DataOffset), Information->DataLength);
        else
            entry.Value = PhReferenceEmptyString();

        //entry.Name = PhCreateStringEx(Information->Name, Information->NameLength);

        PhAddItemArray(Context, &entry);
    }

    return TRUE;
}

static VOID ReadCurrentUserRun(
    VOID
    )
{
    HANDLE keyHandle;

    CurrentUserRunPresent = FALSE;

    if (NT_SUCCESS(PhOpenKey(
        &keyHandle,
        KEY_READ,
        PH_KEY_CURRENT_USER,
        &CurrentUserRunKeyName,
        0
        )))
    {
        PH_ARRAY keyEntryArray;

        PhInitializeArray(&keyEntryArray, sizeof(PHP_HKURUN_ENTRY), 20);
        PhEnumerateValueKey(keyHandle, KeyValueFullInformation, PhpReadCurrentRunCallback, &keyEntryArray);

        for (SIZE_T i = 0; i < PhFinalArrayCount(&keyEntryArray); i++)
        {
            PPHP_HKURUN_ENTRY entry = PhItemArray(&keyEntryArray, i);
            PH_STRINGREF fileName;
            PH_STRINGREF arguments;
            PPH_STRING fullFileName;
            PPH_STRING applicationFileName;

            if (PhParseCommandLineFuzzy(&entry->Value->sr, &fileName, &arguments, &fullFileName))
            {
                if (applicationFileName = PhGetApplicationFileNameWin32())
                {
                    // The full path is compared since an entry that starts another copy of the
                    // application is not this application's startup entry.

                    if (fullFileName && PhEqualString(fullFileName, applicationFileName, TRUE))
                    {
                        CurrentUserRunPresent = TRUE;
                    }

                    PhDereferenceObject(applicationFileName);
                }

                if (fullFileName) PhDereferenceObject(fullFileName);
            }

            PhDereferenceObject(entry->Value);
        }

        PhDeleteArray(&keyEntryArray);

        NtClose(keyHandle);
    }
}

static VOID WriteCurrentUserRun(
    _In_ BOOLEAN Present,
    _In_ BOOLEAN StartHidden
    )
{
    HANDLE keyHandle;

    // The entry is re-read here since the state cached when the dialog opened doesn't reflect
    // the entry being changed or removed while the dialog was open.

    ReadCurrentUserRun();

    if (CurrentUserRunPresent == Present)
        return;

    if (NT_SUCCESS(PhOpenKey(
        &keyHandle,
        KEY_WRITE,
        PH_KEY_CURRENT_USER,
        &CurrentUserRunKeyName,
        0
        )))
    {
        static CONST PH_STRINGREF valueName = PH_STRINGREF_INIT(L"System Informer");
        static CONST PH_STRINGREF separator = PH_STRINGREF_INIT(L"\"");

        if (Present)
        {
            PPH_STRING value;
            PPH_STRING fileName;

            if (fileName = PhGetApplicationFileNameWin32())
            {
                value = PH_AUTO(PhConcatStringRef3(&separator, &fileName->sr, &separator));

                if (StartHidden)
                    value = PH_AUTO(PhConcatStringRefZ(&value->sr, L" -hide"));

                PhSetValueKey(
                    keyHandle,
                    &valueName,
                    REG_SZ,
                    value->Buffer,
                    (ULONG)value->Length + sizeof(UNICODE_NULL)
                    );

                PhDereferenceObject(fileName);
            }
        }
        else
        {
            PhDeleteValueKey(keyHandle, &valueName);
        }

        CurrentUserRunPresent = Present;

        NtClose(keyHandle);
    }
}

static BOOLEAN PathMatchesPh(
    _In_ PPH_STRING Path
    )
{
    BOOLEAN match = FALSE;
    PPH_STRING fileName;

    if (!(fileName = PhGetApplicationFileNameWin32()))
        return FALSE;

    if (PhEqualString(Path, fileName, TRUE))
    {
        match = TRUE;
    }
    // Allow for a quoted value.
    else if (
        Path->Length == fileName->Length + 4 &&
        Path->Buffer[0] == L'"' &&
        Path->Buffer[Path->Length / sizeof(WCHAR) - 1] == L'"'
        )
    {
        PH_STRINGREF partInside;

        partInside.Buffer = &Path->Buffer[1];
        partInside.Length = Path->Length - 2 * sizeof(WCHAR);

        if (PhEqualStringRef(&partInside, &fileName->sr, TRUE))
            match = TRUE;
    }

    PhDereferenceObject(fileName);

    return match;
}

BOOLEAN PhpIsDefaultTaskManager(
    VOID
    )
{
    HANDLE taskmgrKeyHandle;
    BOOLEAN alreadyReplaced = FALSE;

    if (NT_SUCCESS(PhOpenKey(
        &taskmgrKeyHandle,
        KEY_READ,
        PH_KEY_LOCAL_MACHINE,
        &TaskMgrImageOptionsKeyName,
        0
        )))
    {
        PhClearReference(&OldTaskMgrDebugger);

        if (OldTaskMgrDebugger = PhQueryRegistryStringZ(taskmgrKeyHandle, L"Depurador"))
        {
            alreadyReplaced = PathMatchesPh(OldTaskMgrDebugger);
        }

        NtClose(taskmgrKeyHandle);
    }

    return alreadyReplaced;
}

VOID PhpSetDefaultTaskManager(
    _In_ HWND ParentWindowHandle
    )
{
    PWSTR message;

    if (PhpIsDefaultTaskManager())
    {
        message = L"¿Desea restaurar el Administrador de tareas predeterminado de Windows?";
    }
    else
    {
        message = L"¿Desea hacer que System Informer sea el Administrador de tareas predeterminado de Windows?";

        // Warn the user when we're not installed into secure location. (dmex)
        if (!PhShowOptionsDefaultInstallLocation(ParentWindowHandle, L"Cambiar el Administrador de tareas predeterminado"))
        {
            return;
        }
    }

    if (PhShowMessage2(
        ParentWindowHandle,
        TD_YES_BUTTON | TD_NO_BUTTON,
        TD_INFORMATION_ICON,
        L"",
        message
        ) == IDYES)
    {
        NTSTATUS status;
        HANDLE taskmgrKeyHandle;

        status = PhCreateKey(
            &taskmgrKeyHandle,
            KEY_READ | KEY_WRITE,
            PH_KEY_LOCAL_MACHINE,
            &TaskMgrImageOptionsKeyName,
            OBJ_OPENIF,
            0,
            NULL
            );

        if (NT_SUCCESS(status))
        {
            static CONST PH_STRINGREF valueName = PH_STRINGREF_INIT(L"Depurador");
            static CONST PH_STRINGREF separator = PH_STRINGREF_INIT(L"\"");

            if (PhpIsDefaultTaskManager())
            {
                status = PhDeleteValueKey(taskmgrKeyHandle, &valueName);
            }
            else
            {
                PPH_STRING quotedFileName;
                PPH_STRING applicationFileName;

                if (applicationFileName = PhGetApplicationFileNameWin32())
                {
                    quotedFileName = PH_AUTO(PhConcatStringRef3(&separator, &applicationFileName->sr, &separator));

                    status = PhSetValueKey(
                        taskmgrKeyHandle,
                        &valueName,
                        REG_SZ,
                        quotedFileName->Buffer,
                        (ULONG)quotedFileName->Length + sizeof(UNICODE_NULL)
                        );

                    PhDereferenceObject(applicationFileName);
                }
            }

            NtClose(taskmgrKeyHandle);
        }

        if (!NT_SUCCESS(status))
            PhShowStatus(ParentWindowHandle, L"No se puede reemplazar el Administrador de tareas", status, 0);

        //PhSaveSettings2(PhSettingsFileName);
    }
}

//BOOLEAN PhpIsExploitProtectionEnabled(
//    VOID
//    )
//{
//    BOOLEAN enabled = FALSE;
//    HANDLE keyHandle;
//    PPH_STRING directory = NULL;
//    PPH_STRING fileName = NULL;
//    PPH_STRING keyName;
//
//    PhMoveReference(&directory, PhCreateString2(&TaskMgrImageOptionsKeyName));
//    PhMoveReference(&directory, PhGetBaseDirectory(directory));
//    PhMoveReference(&fileName, PhGetApplicationFileNameWin32());
//    PhMoveReference(&fileName, PhGetBaseName(fileName));
//    keyName = PhConcatStringRef3(&directory->sr, &PhNtPathSeparatorString, &fileName->sr);
//
//    if (NT_SUCCESS(PhOpenKey(
//        &keyHandle,
//        KEY_READ,
//        PH_KEY_LOCAL_MACHINE,
//        &keyName->sr,
//        0
//        )))
//    {
//        PH_STRINGREF valueName;
//        PKEY_VALUE_PARTIAL_INFORMATION buffer;
//
//        enabled = !(PhQueryRegistryUlong64(keyHandle, L"MitigationOptions") == ULLONG_MAX);
//        PhInitializeStringRef(&valueName, L"MitigationOptions");
//
//        if (NT_SUCCESS(PhQueryValueKey(keyHandle, &valueName, KeyValuePartialInformation, &buffer)))
//        {
//            if (buffer->Type == REG_BINARY && buffer->DataLength)
//            {
//                enabled = TRUE;
//            }
//
//            PhFree(buffer);
//        }
//
//        NtClose(keyHandle);
//    }
//
//    PhDereferenceObject(keyName);
//    PhDereferenceObject(fileName);
//    PhDereferenceObject(directory);
//
//    return enabled;
//}
//
//NTSTATUS PhpSetExploitProtectionEnabled(
//    _In_ BOOLEAN Enabled)
//{
//    static PH_STRINGREF replacementToken = PH_STRINGREF_INIT(L"Software\\");
//    static PH_STRINGREF wow6432Token = PH_STRINGREF_INIT(L"Software\\WOW6432Node\\");
//    static PH_STRINGREF valueName = PH_STRINGREF_INIT(L"MitigationOptions");
//    NTSTATUS status = STATUS_UNSUCCESSFUL;
//    HANDLE keyHandle;
//    PPH_STRING directory;
//    PPH_STRING fileName;
//    PPH_STRING keyName;
//
//    directory = PhCreateString2(&TaskMgrImageOptionsKeyName);
//    PhMoveReference(&directory, PhGetBaseDirectory(directory));
//    fileName = PhGetApplicationFileNameWin32();
//    PhMoveReference(&fileName, PhGetBaseName(fileName));
//    keyName = PhConcatStringRef3(&directory->sr, &PhNtPathSeparatorString, &fileName->sr);
//
//    //if (Enabled)
//    //{
//    //    status = PhCreateKey(
//    //        &keyHandle,
//    //        KEY_WRITE,
//    //        PH_KEY_LOCAL_MACHINE,
//    //        &keyName->sr,
//    //        OBJ_OPENIF,
//    //        0,
//    //        NULL
//    //        );
//    //
//    //    if (NT_SUCCESS(status))
//    //    {
//    //        status = PhSetValueKey(keyHandle, &valueName, REG_QWORD, &(ULONG64)
//    //        {
//    //            PROCESS_CREATION_MITIGATION_POLICY_HEAP_TERMINATE_ALWAYS_ON |
//    //            PROCESS_CREATION_MITIGATION_POLICY_BOTTOM_UP_ASLR_ALWAYS_ON |
//    //            PROCESS_CREATION_MITIGATION_POLICY_HIGH_ENTROPY_ASLR_ALWAYS_ON |
//    //            PROCESS_CREATION_MITIGATION_POLICY_EXTENSION_POINT_DISABLE_ALWAYS_ON |
//    //            PROCESS_CREATION_MITIGATION_POLICY_PROHIBIT_DYNAMIC_CODE_ALWAYS_ON |
//    //            PROCESS_CREATION_MITIGATION_POLICY_CONTROL_FLOW_GUARD_ALWAYS_ON |
//    //            PROCESS_CREATION_MITIGATION_POLICY_FONT_DISABLE_ALWAYS_ON |
//    //            PROCESS_CREATION_MITIGATION_POLICY_IMAGE_LOAD_NO_REMOTE_ALWAYS_ON |
//    //            PROCESS_CREATION_MITIGATION_POLICY_IMAGE_LOAD_NO_LOW_LABEL_ALWAYS_ON |
//    //            PROCESS_CREATION_MITIGATION_POLICY_IMAGE_LOAD_PREFER_SYSTEM32_ALWAYS_ON,
//    //        }, sizeof(ULONG64));
//    //
//    //        NtClose(keyHandle);
//    //    }
//    //}
//    //else
//    {
//        status = PhOpenKey(
//            &keyHandle,
//            KEY_WRITE,
//            PH_KEY_LOCAL_MACHINE,
//            &keyName->sr,
//            OBJ_OPENIF
//            );
//
//        if (NT_SUCCESS(status))
//        {
//            status = PhDeleteValueKey(keyHandle, &valueName);
//            NtClose(keyHandle);
//        }
//
//        if (status == STATUS_OBJECT_NAME_NOT_FOUND)
//            status = STATUS_SUCCESS;
//
//#ifdef _WIN64
//        if (NT_SUCCESS(status))
//        {
//            PH_STRINGREF stringBefore;
//            PH_STRINGREF stringAfter;
//
//            if (PhSplitStringRefAtString(&keyName->sr, &replacementToken, TRUE, &stringBefore, &stringAfter))
//            {
//                PhMoveReference(&keyName, PhConcatStringRef3(&stringBefore, &wow6432Token, &stringAfter));
//
//                status = PhOpenKey(
//                    &keyHandle,
//                    DELETE,
//                    PH_KEY_LOCAL_MACHINE,
//                    &keyName->sr,
//                    OBJ_OPENIF
//                    );
//
//                if (NT_SUCCESS(status))
//                {
//                    status = NtDeleteKey(keyHandle);
//                    NtClose(keyHandle);
//                }
//            }
//        }
//#endif
//    }
//
//    if (status == STATUS_OBJECT_NAME_NOT_FOUND)
//        status = STATUS_SUCCESS;
//
//    PhDereferenceObject(keyName);
//    PhDereferenceObject(fileName);
//    PhDereferenceObject(directory);
//
//    return status;
//}

NTSTATUS PhpSetSilentProcessNotifyEnabled(
    _In_ BOOLEAN Enabled)
{
    static CONST PH_STRINGREF processExitKeyName = PH_STRINGREF_INIT(L"Software\\Microsoft\\Windows NT\\CurrentVersion\\SilentProcessExit");
    static CONST PH_STRINGREF valueModeName = PH_STRINGREF_INIT(L"ReportingMode");
    static ULONG valueMode = 4;
    //static CONST PH_STRINGREF valueSelfName = PH_STRINGREF_INIT(L"IgnoreSelfExits");
    //static ULONG valueSelf = 1;
    //static CONST PH_STRINGREF valueMonitorName = PH_STRINGREF_INIT(L"MonitorProcess");
    static CONST PH_STRINGREF valueGlobalName = PH_STRINGREF_INIT(L"GlobalFlag");
    NTSTATUS status = STATUS_UNSUCCESSFUL;
    HANDLE keyDirectoryHandle = NULL;
    HANDLE keyFilenameHandle = NULL;
    PPH_STRING filename;
    PPH_STRING baseName;

    filename = PH_AUTO(PhGetApplicationFileNameWin32());
    baseName = PH_AUTO(PhGetBaseName(filename));

    if (Enabled)
    {
        status = PhCreateKey(
            &keyDirectoryHandle,
            KEY_WRITE,
            PH_KEY_LOCAL_MACHINE,
            &processExitKeyName,
            OBJ_OPENIF,
            0,
            NULL
            );

        if (!NT_SUCCESS(status))
            goto CleanupExit;

        status = PhCreateKey(
            &keyFilenameHandle,
            KEY_WRITE,
            keyDirectoryHandle,
            &baseName->sr,
            OBJ_OPENIF,
            0,
            NULL
            );

        if (!NT_SUCCESS(status))
            goto CleanupExit;

        status = PhSetValueKey(
            keyFilenameHandle,
            &valueModeName,
            REG_DWORD,
            &valueMode,
            sizeof(ULONG)
            );

        if (!NT_SUCCESS(status))
            goto CleanupExit;

        //PhSetValueKey(keyFilenameHandle, &valueSelfName, REG_DWORD, &valueSelf, sizeof(ULONG));
        //PhSetValueKey(keyFilenameHandle, &valueMonitorName, REG_SZ, filename->Buffer, (ULONG)filename->Length + sizeof(UNICODE_NULL));

        if (NT_SUCCESS(status))
        {
            HANDLE keyHandle;
            PPH_STRING directory;
            PPH_STRING fileName;
            PPH_STRING keyName;

            directory = PH_AUTO(PhCreateString2(&TaskMgrImageOptionsKeyName));
            directory = PH_AUTO(PhGetBaseDirectory(directory));
            fileName = PH_AUTO(PhGetApplicationFileNameWin32());
            fileName = PH_AUTO(PhGetBaseName(fileName));
            keyName = PH_AUTO(PhConcatStringRef3(&directory->sr, &PhNtPathSeparatorString, &fileName->sr));

            status = PhCreateKey(
                &keyHandle,
                KEY_WRITE,
                PH_KEY_LOCAL_MACHINE,
                &keyName->sr,
                OBJ_OPENIF,
                0,
                NULL
                );

            if (NT_SUCCESS(status))
            {
                ULONG globalFlags = PhQueryRegistryUlongZ(keyHandle, L"GlobalFlag");

                if (globalFlags == ULONG_MAX) globalFlags = 0;
                globalFlags = globalFlags | FLG_MONITOR_SILENT_PROCESS_EXIT;

                status = PhSetValueKey(keyHandle, &valueGlobalName, REG_DWORD, &globalFlags, sizeof(ULONG));
                NtClose(keyHandle);
            }
        }
    }
    else
    {
        if (NT_SUCCESS(PhOpenKey(
            &keyDirectoryHandle,
            DELETE,
            PH_KEY_LOCAL_MACHINE,
            &processExitKeyName,
            0
            )))
        {
            if (NT_SUCCESS(PhOpenKey(
                &keyFilenameHandle,
                DELETE,
                keyDirectoryHandle,
                &baseName->sr,
                0
                )))
            {
                NtDeleteKey(keyFilenameHandle);
            }
        }

        {
            HANDLE keyHandle;
            PPH_STRING directory;
            PPH_STRING fileName;
            PPH_STRING keyName;

            directory = PH_AUTO(PhCreateString2(&TaskMgrImageOptionsKeyName));
            directory = PH_AUTO(PhGetBaseDirectory(directory));
            fileName = PH_AUTO(PhGetApplicationFileNameWin32());
            fileName = PH_AUTO(PhGetBaseName(fileName));
            keyName = PH_AUTO(PhConcatStringRef3(&directory->sr, &PhNtPathSeparatorString, &fileName->sr));

            status = PhOpenKey(
                &keyHandle,
                KEY_WRITE,
                PH_KEY_LOCAL_MACHINE,
                &keyName->sr,
                0
                );

            if (NT_SUCCESS(status))
            {
                status = PhDeleteValueKey(keyHandle, &valueGlobalName);
                NtClose(keyHandle);
            }
        }
    }

CleanupExit:
    if (keyDirectoryHandle)
        NtClose(keyDirectoryHandle);
    if (keyFilenameHandle)
        NtClose(keyFilenameHandle);

    return status;
}

VOID PhpRefreshTaskManagerState(
    _In_ HWND WindowHandle
    )
{
    if (!PhGetOwnTokenAttributes().Elevated)
    {
        Button_SetElevationRequiredState(GetDlgItem(WindowHandle, IDC_REPLACETASKMANAGER), TRUE);
    }

    if (PhpIsDefaultTaskManager())
    {
        PhSetWindowText(GetDlgItem(WindowHandle, IDC_DEFSTATE), L"System Informer es el Administrador de tareas predeterminado:");
        PhSetWindowText(GetDlgItem(WindowHandle, IDC_REPLACETASKMANAGER), L"Restaurar valores predeterminados...");
    }
    else
    {
        PhSetWindowText(GetDlgItem(WindowHandle, IDC_DEFSTATE), L"System Informer no es el Administrador de tareas predeterminado:");
        PhSetWindowText(GetDlgItem(WindowHandle, IDC_REPLACETASKMANAGER), L"Establecer como predeterminado...");
    }
}

typedef enum _PHP_OPTIONS_INDEX
{
    PHP_OPTIONS_INDEX_SINGLE_INSTANCE,
    PHP_OPTIONS_INDEX_HIDE_WHENCLOSED,
    PHP_OPTIONS_INDEX_HIDE_WHENMINIMIZED,
    PHP_OPTIONS_INDEX_START_ATLOGON,
    PHP_OPTIONS_INDEX_START_HIDDEN,
    PHP_OPTIONS_INDEX_ENABLE_WARNINGS,
    PHP_OPTIONS_INDEX_ENABLE_DRIVER,
    PHP_OPTIONS_INDEX_ENABLE_MONOSPACE,
    PHP_OPTIONS_INDEX_ENABLE_PLUGINS,
    PHP_OPTIONS_INDEX_ENABLE_AVX_EXTENSIONS,
    PHP_OPTIONS_INDEX_ENABLE_UNDECORATE_SYMBOLS,
    PHP_OPTIONS_INDEX_ENABLE_COLUMN_HEADER_TOTALS,
    PHP_OPTIONS_INDEX_ENABLE_CYCLE_CPU_USAGE,
    PHP_OPTIONS_INDEX_ENABLE_LOW_LATENCY_MODE,
    PHP_OPTIONS_INDEX_ENABLE_GRAPH_SCALING,
    PHP_OPTIONS_INDEX_ENABLE_MINIINFO_WINDOW,
    PHP_OPTIONS_INDEX_ENABLE_MEMSTRINGS_TREE,
    PHP_OPTIONS_INDEX_ENABLE_LASTTAB_SUPPORT,
    PHP_OPTIONS_INDEX_ENABLE_THEME_SUPPORT,
    PHP_OPTIONS_INDEX_ENABLE_START_ASADMIN,
    PHP_OPTIONS_INDEX_ENABLE_STREAM_MODE,
    PHP_OPTIONS_INDEX_ENABLE_NETWORK_RESOLVE,
    PHP_OPTIONS_INDEX_ENABLE_NETWORK_RESOLVE_DOH,
    PHP_OPTIONS_INDEX_ENABLE_INSTANT_TOOLTIPS,
    PHP_OPTIONS_INDEX_ENABLE_IMAGE_COHERENCY,
    PHP_OPTIONS_INDEX_ENABLE_STAGE2,
    PHP_OPTIONS_INDEX_ENABLE_SERVICE_STAGE2,
    PHP_OPTIONS_INDEX_ICON_SINGLE_CLICK,
    PHP_OPTIONS_INDEX_ICON_TOGGLE_VISIBILITY,
    PHP_OPTIONS_INDEX_PROPAGATE_CPU_USAGE,
    PHP_OPTIONS_INDEX_PROCESS_MONITOR,
    PHP_OPTIONS_INDEX_SHOW_ADVANCED_OPTIONS
} PHP_OPTIONS_GENERAL_INDEX;

static VOID PhpAdvancedPageLoad(
    _In_ HWND hwndDlg,
    _In_ BOOLEAN ReloadOnly
    )
{
    HWND listViewHandle;

    listViewHandle = GetDlgItem(hwndDlg, IDC_SETTINGS);

    PhSetDialogItemValue(hwndDlg, IDC_SAMPLECOUNT, PhGetIntegerSetting(SETTING_SAMPLE_COUNT), FALSE);
    SetDlgItemCheckForSetting(hwndDlg, IDC_SAMPLECOUNTAUTOMATIC, SETTING_SAMPLE_COUNT_AUTOMATIC);

    if (PhGetIntegerSetting(SETTING_SAMPLE_COUNT_AUTOMATIC))
        EnableWindow(GetDlgItem(hwndDlg, IDC_SAMPLECOUNT), FALSE);

    if (!ReloadOnly)
    {
        PhAddListViewItem(listViewHandle, PHP_OPTIONS_INDEX_SINGLE_INSTANCE, L"Permitir solo una instancia", NULL);
        PhAddListViewItem(listViewHandle, PHP_OPTIONS_INDEX_HIDE_WHENCLOSED, L"Ocultar al cerrar", NULL);
        PhAddListViewItem(listViewHandle, PHP_OPTIONS_INDEX_HIDE_WHENMINIMIZED, L"Ocultar al minimizar", NULL);
        PhAddListViewItem(listViewHandle, PHP_OPTIONS_INDEX_START_ATLOGON, L"Iniciar cuando inicio sesión", NULL);
        PhAddListViewItem(listViewHandle, PHP_OPTIONS_INDEX_START_HIDDEN, L"Iniciar oculto", NULL);
        PhAddListViewItem(listViewHandle, PHP_OPTIONS_INDEX_ENABLE_WARNINGS, L"Habilitar advertencias", NULL);
        PhAddListViewItem(listViewHandle, PHP_OPTIONS_INDEX_ENABLE_DRIVER, L"Habilitar controlador en modo kernel", NULL);
        PhAddListViewItem(listViewHandle, PHP_OPTIONS_INDEX_ENABLE_MONOSPACE, L"Habilitar fuentes monoespaciadas", NULL);
        PhAddListViewItem(listViewHandle, PHP_OPTIONS_INDEX_ENABLE_PLUGINS, L"Habilitar complementos", NULL);
        PhAddListViewItem(listViewHandle, PHP_OPTIONS_INDEX_ENABLE_UNDECORATE_SYMBOLS, L"Habilitar símbolos sin decorar", NULL);
        PhAddListViewItem(listViewHandle, PHP_OPTIONS_INDEX_ENABLE_AVX_EXTENSIONS, L"Habilitar extensiones AVX (experimental)", NULL);
        PhAddListViewItem(listViewHandle, PHP_OPTIONS_INDEX_ENABLE_COLUMN_HEADER_TOTALS, L"Habilitar totales en encabezados de columna (experimental)", NULL);
        PhAddListViewItem(listViewHandle, PHP_OPTIONS_INDEX_ENABLE_CYCLE_CPU_USAGE, L"Habilitar uso de CPU basado en ciclos", NULL);
        PhAddListViewItem(listViewHandle, PHP_OPTIONS_INDEX_ENABLE_LOW_LATENCY_MODE, L"Habilitar modo de baja latencia (experimental)", NULL);
        PhAddListViewItem(listViewHandle, PHP_OPTIONS_INDEX_ENABLE_GRAPH_SCALING, L"Habilitar escala fija de gráficos (experimental)", NULL);
        PhAddListViewItem(listViewHandle, PHP_OPTIONS_INDEX_ENABLE_MINIINFO_WINDOW, L"Habilitar ventana de información de la bandeja", NULL);
        PhAddListViewItem(listViewHandle, PHP_OPTIONS_INDEX_ENABLE_MEMSTRINGS_TREE, L"Habilitar el nuevo diálogo de cadenas de memoria", NULL);
        PhAddListViewItem(listViewHandle, PHP_OPTIONS_INDEX_ENABLE_LASTTAB_SUPPORT, L"Recordar la última ventana seleccionada", NULL);
        PhAddListViewItem(listViewHandle, PHP_OPTIONS_INDEX_ENABLE_THEME_SUPPORT, L"Habilitar soporte de temas (experimental)", NULL);
        PhAddListViewItem(listViewHandle, PHP_OPTIONS_INDEX_ENABLE_START_ASADMIN, L"Habilitar inicio como administrador (experimental)", NULL);
        PhAddListViewItem(listViewHandle, PHP_OPTIONS_INDEX_ENABLE_STREAM_MODE, L"Habilitar modo streamer (deshabilitar captura de ventana) (experimental)", NULL);
        //PhAddListViewItem(listViewHandle, PHP_OPTIONS_INDEX_ENABLE_LINUX_SUPPORT, L"Habilitar compatibilidad con el Subsistema de Windows para Linux", NULL);
        PhAddListViewItem(listViewHandle, PHP_OPTIONS_INDEX_ENABLE_NETWORK_RESOLVE, L"Resolver direcciones de red", NULL);
        PhAddListViewItem(listViewHandle, PHP_OPTIONS_INDEX_ENABLE_NETWORK_RESOLVE_DOH, L"Resolver DNS sobre HTTPS (DoH)", NULL);
        PhAddListViewItem(listViewHandle, PHP_OPTIONS_INDEX_ENABLE_INSTANT_TOOLTIPS, L"Mostrar descripciones emergentes al instante", NULL);
        PhAddListViewItem(listViewHandle, PHP_OPTIONS_INDEX_ENABLE_IMAGE_COHERENCY, L"Comprobar la coherencia de las imágenes", NULL);
        PhAddListViewItem(listViewHandle, PHP_OPTIONS_INDEX_ENABLE_STAGE2, L"Comprobar las firmas digitales de las imágenes", NULL);
        PhAddListViewItem(listViewHandle, PHP_OPTIONS_INDEX_ENABLE_SERVICE_STAGE2, L"Comprobar las firmas digitales de los servicios", NULL);
        PhAddListViewItem(listViewHandle, PHP_OPTIONS_INDEX_ICON_SINGLE_CLICK, L"Iconos de la bandeja con un solo clic", NULL);
        PhAddListViewItem(listViewHandle, PHP_OPTIONS_INDEX_ICON_TOGGLE_VISIBILITY, L"Al hacer clic en el icono se alterna la visibilidad", NULL);
        PhAddListViewItem(listViewHandle, PHP_OPTIONS_INDEX_PROPAGATE_CPU_USAGE, L"Incluir el uso de los procesos contraídos", NULL);
        if (WindowsVersion >= WINDOWS_10)
            PhAddListViewItem(listViewHandle, PHP_OPTIONS_INDEX_PROCESS_MONITOR, L"Habilitar monitor de procesos (experimental)", NULL);
        PhAddListViewItem(listViewHandle, PHP_OPTIONS_INDEX_SHOW_ADVANCED_OPTIONS, L"Mostrar opciones avanzadas", NULL);
    }

    SetLvItemCheckForSetting(listViewHandle, PHP_OPTIONS_INDEX_SINGLE_INSTANCE, SETTING_ALLOW_ONLY_ONE_INSTANCE);
    SetLvItemCheckForSetting(listViewHandle, PHP_OPTIONS_INDEX_HIDE_WHENCLOSED, SETTING_HIDE_ON_CLOSE);
    SetLvItemCheckForSetting(listViewHandle, PHP_OPTIONS_INDEX_HIDE_WHENMINIMIZED, SETTING_HIDE_ON_MINIMIZE);
    SetLvItemCheckForSetting(listViewHandle, PHP_OPTIONS_INDEX_START_HIDDEN, SETTING_START_HIDDEN);
    SetLvItemCheckForSetting(listViewHandle, PHP_OPTIONS_INDEX_ENABLE_MINIINFO_WINDOW, SETTING_MINI_INFO_WINDOW_ENABLED);
    SetLvItemCheckForSetting(listViewHandle, PHP_OPTIONS_INDEX_ENABLE_MEMSTRINGS_TREE, SETTING_ENABLE_MEM_STRINGS_TREE_DIALOG);
    SetLvItemCheckForSetting(listViewHandle, PHP_OPTIONS_INDEX_ENABLE_LASTTAB_SUPPORT, SETTING_MAIN_WINDOW_TAB_RESTORE_ENABLED);
    SetLvItemCheckForSetting(listViewHandle, PHP_OPTIONS_INDEX_ENABLE_DRIVER, SETTING_KSI_ENABLE);
    SetLvItemCheckForSetting(listViewHandle, PHP_OPTIONS_INDEX_ENABLE_WARNINGS, SETTING_ENABLE_WARNINGS);
    SetLvItemCheckForSetting(listViewHandle, PHP_OPTIONS_INDEX_ENABLE_PLUGINS, SETTING_ENABLE_PLUGINS);
    SetLvItemCheckForSetting(listViewHandle, PHP_OPTIONS_INDEX_ENABLE_UNDECORATE_SYMBOLS, SETTING_DBGHELP_UNDECORATE);
    SetLvItemCheckForSetting(listViewHandle, PHP_OPTIONS_INDEX_ENABLE_AVX_EXTENSIONS, SETTING_ENABLE_AVX_SUPPORT);
    SetLvItemCheckForSetting(listViewHandle, PHP_OPTIONS_INDEX_ENABLE_COLUMN_HEADER_TOTALS, SETTING_TREE_LIST_ENABLE_HEADER_TOTALS);
    SetLvItemCheckForSetting(listViewHandle, PHP_OPTIONS_INDEX_ENABLE_GRAPH_SCALING, SETTING_ENABLE_GRAPH_MAX_SCALE);
    SetLvItemCheckForSetting(listViewHandle, PHP_OPTIONS_INDEX_ENABLE_CYCLE_CPU_USAGE, SETTING_ENABLE_CYCLE_CPU_USAGE);
    SetLvItemCheckForSetting(listViewHandle, PHP_OPTIONS_INDEX_ENABLE_LOW_LATENCY_MODE, SETTING_ENABLE_HIGH_RESOLUTION);
    SetLvItemCheckForSetting(listViewHandle, PHP_OPTIONS_INDEX_ENABLE_THEME_SUPPORT, SETTING_ENABLE_THEME_SUPPORT);
    SetLvItemCheckForSetting(listViewHandle, PHP_OPTIONS_INDEX_ENABLE_START_ASADMIN, SETTING_ENABLE_START_AS_ADMIN);
    SetLvItemCheckForSetting(listViewHandle, PHP_OPTIONS_INDEX_ENABLE_STREAM_MODE, SETTING_ENABLE_STREAMER_MODE);
    SetLvItemCheckForSetting(listViewHandle, PHP_OPTIONS_INDEX_ENABLE_MONOSPACE, SETTING_ENABLE_MONOSPACE_FONT);
    //SetLvItemCheckForSetting(listViewHandle, PHP_OPTIONS_INDEX_ENABLE_LINUX_SUPPORT, SETTING_ENABLE_LINUX_SUBSYSTEM_SUPPORT);
    SetLvItemCheckForSetting(listViewHandle, PHP_OPTIONS_INDEX_ENABLE_NETWORK_RESOLVE, SETTING_ENABLE_NETWORK_RESOLVE);
    SetLvItemCheckForSetting(listViewHandle, PHP_OPTIONS_INDEX_ENABLE_NETWORK_RESOLVE_DOH, SETTING_ENABLE_NETWORK_RESOLVE_DOH);
    SetLvItemCheckForSetting(listViewHandle, PHP_OPTIONS_INDEX_ENABLE_INSTANT_TOOLTIPS, SETTING_ENABLE_INSTANT_TOOLTIPS);
    SetLvItemCheckForSetting(listViewHandle, PHP_OPTIONS_INDEX_ENABLE_IMAGE_COHERENCY, SETTING_ENABLE_IMAGE_COHERENCY_SUPPORT);
    SetLvItemCheckForSetting(listViewHandle, PHP_OPTIONS_INDEX_ENABLE_STAGE2, SETTING_ENABLE_STAGE2);
    SetLvItemCheckForSetting(listViewHandle, PHP_OPTIONS_INDEX_ENABLE_SERVICE_STAGE2, SETTING_ENABLE_SERVICE_STAGE2);
    SetLvItemCheckForSetting(listViewHandle, PHP_OPTIONS_INDEX_ICON_SINGLE_CLICK, SETTING_ICON_SINGLE_CLICK);
    SetLvItemCheckForSetting(listViewHandle, PHP_OPTIONS_INDEX_ICON_TOGGLE_VISIBILITY, SETTING_ICON_TOGGLES_VISIBILITY);
    SetLvItemCheckForSetting(listViewHandle, PHP_OPTIONS_INDEX_PROPAGATE_CPU_USAGE, SETTING_PROPAGATE_CPU_USAGE);
    SetLvItemCheckForSetting(listViewHandle, PHP_OPTIONS_INDEX_SHOW_ADVANCED_OPTIONS, SETTING_ENABLE_ADVANCED_OPTIONS);
    if (WindowsVersion >= WINDOWS_10)
        SetLvItemCheckForSetting(listViewHandle, PHP_OPTIONS_INDEX_PROCESS_MONITOR, SETTING_ENABLE_PROCESS_MONITOR);

    if (CurrentUserRunPresent)
        ListView_SetCheckState(listViewHandle, PHP_OPTIONS_INDEX_START_ATLOGON, TRUE);
    if (PhGetIntegerSetting(SETTING_ENABLE_ADVANCED_OPTIONS))
        PhpOptionsShowHideTreeViewItem(FALSE);
}

static VOID PhpOptionsNotifyChangeCallback(
    _In_ PVOID Context
    )
{
    PhUpdateCachedSettings();
    SystemInformer_SaveAllSettings();
    PhInvalidateAllProcessNodes();
    PhReloadSettingsProcessTreeList();
    PhSiNotifyChangeSettings();

    //PhReInitializeWindowTheme(PhMainWndHandle);

    PhInvokeCallback(PhGetGeneralCallback(GeneralCallbackSettingsUpdated), &RestartRequired);

    if (RestartRequired)
    {
        if (PhShowMessage2(
            PhMainWndHandle,
            TD_YES_BUTTON | TD_NO_BUTTON,
            TD_INFORMATION_ICON,
            L"Una o más opciones que ha cambiado requieren reiniciar System Informer.",
            L"¿Desea reiniciar System Informer ahora?"
            ) == IDYES)
        {
            SystemInformer_PrepareForEarlyShutdown();

            if (NT_SUCCESS(PhShellProcessHacker(
                PhMainWndHandle,
                L"-v -newinstance",
                SW_SHOW,
                PH_SHELL_EXECUTE_DEFAULT,
                PH_SHELL_APP_PROPAGATE_PARAMETERS | PH_SHELL_APP_PROPAGATE_PARAMETERS_IGNORE_VISIBILITY,
                0,
                NULL
                )))
            {
                SystemInformer_Destroy();
            }
            else
            {
                SystemInformer_CancelEarlyShutdown();
            }
        }
    }
}

VOID PhShowOptionsRestartRequired(
    _In_ HWND WindowHandle
    )
{
    HWND ownerWindowHandle;

    ownerWindowHandle = WindowHandle && IsWindow(WindowHandle) ? WindowHandle : PhMainWndHandle;

    if (PhShowMessage2(
        ownerWindowHandle,
        TD_YES_BUTTON | TD_NO_BUTTON,
        TD_INFORMATION_ICON,
        L"Una o más opciones que ha cambiado requieren reiniciar System Informer.",
        L"¿Desea reiniciar System Informer ahora?"
        ) == IDYES)
    {
        SystemInformer_PrepareForEarlyShutdown();

        if (NT_SUCCESS(PhShellProcessHacker(
            ownerWindowHandle,
            L"-v -newinstance",
            SW_SHOW,
            PH_SHELL_EXECUTE_DEFAULT,
            PH_SHELL_APP_PROPAGATE_PARAMETERS | PH_SHELL_APP_PROPAGATE_PARAMETERS_IGNORE_VISIBILITY,
            0,
            NULL
            )))
        {
            SystemInformer_Destroy();
        }
        else
        {
            SystemInformer_CancelEarlyShutdown();
        }
    }
}

BOOLEAN PhShowOptionsDefaultInstallLocation(
    _In_ HWND ParentWindowHandle,
    _In_ PCWSTR Message
    )
{
    RTL_ELEVATION_FLAGS flags;

    // Warn the user when we're not installed into secure location. (dmex)
    if (NT_SUCCESS(RtlQueryElevationFlags(&flags)) && flags.ElevationEnabled)
    {
        PPH_STRING applicationFileName;
        PPH_STRING programFilesPath;

        if (applicationFileName = PhGetApplicationFileNameWin32())
        {
            if (programFilesPath = PhExpandEnvironmentStringsZ(L"%ProgramFiles%\\"))
            {
                if (!PhStartsWithString(applicationFileName, programFilesPath, TRUE))
                {
                    if (PhShowMessage2(
                        ParentWindowHandle,
                        TD_YES_BUTTON | TD_NO_BUTTON,
                        TD_WARNING_ICON,
                        L"ADVERTENCIA: No ha instalado System Informer en una ubicación segura.",
                        L"%s is not recommended when running System Informer from outside a secure location (e.g. Program Files).\r\n\r\nAre you sure you want to continue?",
                        Message
                        ) == IDNO)
                    {
                        PhDereferenceObject(programFilesPath);
                        PhDereferenceObject(applicationFileName);
                        return FALSE;
                    }
                }

                PhDereferenceObject(programFilesPath);
            }

            PhDereferenceObject(applicationFileName);
        }
    }

    return TRUE;
}

static VOID PhpAdvancedPageSave(
    _In_ HWND hwndDlg
    )
{
    HWND listViewHandle;
    ULONG sampleCount;
    BOOLEAN themeSupportWasEnabled;

    if (!hwndDlg)
        return;

    themeSupportWasEnabled = !!PhGetIntegerSetting(SETTING_ENABLE_THEME_SUPPORT);

    listViewHandle = GetDlgItem(hwndDlg, IDC_SETTINGS);
    sampleCount = PhGetDialogItemValue(hwndDlg, IDC_SAMPLECOUNT);

    SetSettingForDlgItemCheckRestartRequired(hwndDlg, IDC_SAMPLECOUNTAUTOMATIC, SETTING_SAMPLE_COUNT_AUTOMATIC);

    if (sampleCount == 0)
        sampleCount = 1;

    if (sampleCount != PhGetIntegerSetting(SETTING_SAMPLE_COUNT))
        RestartRequired = TRUE;

    PhSetIntegerSetting(SETTING_SAMPLE_COUNT, sampleCount);
    PhSetStringSetting2(SETTING_SEARCH_ENGINE, &PhaGetDlgItemText(hwndDlg, IDC_SEARCHENGINE)->sr);
    PhSetStringSetting2(SETTING_PROGRAM_INSPECT_EXECUTABLES, &PhaGetDlgItemText(hwndDlg, IDC_PEVIEWER)->sr);
    PhSetIntegerSetting(SETTING_MAX_SIZE_UNIT, PhMaxSizeUnit = ComboBox_GetCurSel(GetDlgItem(hwndDlg, IDC_MAXSIZEUNIT)));
    PhSetIntegerSetting(SETTING_ICON_PROCESSES, PhGetDialogItemValue(hwndDlg, IDC_ICONPROCESSES));

    if (!PhEqualString(PhaGetDlgItemText(hwndDlg, IDC_DBGHELPSEARCHPATH), PhaGetStringSetting(SETTING_DBGHELP_SEARCH_PATH), TRUE))
    {
        PhSetStringSetting2(SETTING_DBGHELP_SEARCH_PATH, &PhaGetDlgItemText(hwndDlg, IDC_DBGHELPSEARCHPATH)->sr);
        RestartRequired = TRUE;
    }

    // When changing driver enabled setting, it only makes sense to require a restart if we're
    // already elevated. If we're not elevated and asked to restart, we would not connect to the
    // driver and the user has to elevate (restart) again anyway. (jxy-s)
    if (PhGetOwnTokenAttributes().Elevated)
    {
        SetSettingForLvItemCheckRestartRequired(listViewHandle, PHP_OPTIONS_INDEX_ENABLE_DRIVER, SETTING_KSI_ENABLE);
    }
    else
    {
        SetSettingForLvItemCheck(listViewHandle, PHP_OPTIONS_INDEX_ENABLE_DRIVER, SETTING_KSI_ENABLE);
    }

    SetSettingForLvItemCheck(listViewHandle, PHP_OPTIONS_INDEX_SINGLE_INSTANCE, SETTING_ALLOW_ONLY_ONE_INSTANCE);
    SetSettingForLvItemCheck(listViewHandle, PHP_OPTIONS_INDEX_HIDE_WHENCLOSED, SETTING_HIDE_ON_CLOSE);
    SetSettingForLvItemCheck(listViewHandle, PHP_OPTIONS_INDEX_HIDE_WHENMINIMIZED, SETTING_HIDE_ON_MINIMIZE);
    SetSettingForLvItemCheck(listViewHandle, PHP_OPTIONS_INDEX_START_HIDDEN, SETTING_START_HIDDEN);
    SetSettingForLvItemCheckRestartRequired(listViewHandle, PHP_OPTIONS_INDEX_ENABLE_MINIINFO_WINDOW, SETTING_MINI_INFO_WINDOW_ENABLED);
    SetSettingForLvItemCheck(listViewHandle, PHP_OPTIONS_INDEX_ENABLE_MEMSTRINGS_TREE, SETTING_ENABLE_MEM_STRINGS_TREE_DIALOG);
    SetSettingForLvItemCheck(listViewHandle, PHP_OPTIONS_INDEX_ENABLE_LASTTAB_SUPPORT, SETTING_MAIN_WINDOW_TAB_RESTORE_ENABLED);
    SetSettingForLvItemCheck(listViewHandle, PHP_OPTIONS_INDEX_ENABLE_WARNINGS, SETTING_ENABLE_WARNINGS);
    SetSettingForLvItemCheckRestartRequired(listViewHandle, PHP_OPTIONS_INDEX_ENABLE_PLUGINS, SETTING_ENABLE_PLUGINS);
    SetSettingForLvItemCheck(listViewHandle, PHP_OPTIONS_INDEX_ENABLE_UNDECORATE_SYMBOLS, SETTING_DBGHELP_UNDECORATE);
    SetSettingForLvItemCheck(listViewHandle, PHP_OPTIONS_INDEX_ENABLE_AVX_EXTENSIONS, SETTING_ENABLE_AVX_SUPPORT);
    SetSettingForLvItemCheckRestartRequired(listViewHandle, PHP_OPTIONS_INDEX_ENABLE_COLUMN_HEADER_TOTALS, SETTING_TREE_LIST_ENABLE_HEADER_TOTALS);
    SetSettingForLvItemCheckRestartRequired(listViewHandle, PHP_OPTIONS_INDEX_ENABLE_GRAPH_SCALING, SETTING_ENABLE_GRAPH_MAX_SCALE);
    SetSettingForLvItemCheckRestartRequired(listViewHandle, PHP_OPTIONS_INDEX_ENABLE_CYCLE_CPU_USAGE, SETTING_ENABLE_CYCLE_CPU_USAGE);
    SetSettingForLvItemCheckRestartRequired(listViewHandle, PHP_OPTIONS_INDEX_ENABLE_LOW_LATENCY_MODE, SETTING_ENABLE_HIGH_RESOLUTION);
    SetSettingForLvItemCheckRestartRequired(listViewHandle, PHP_OPTIONS_INDEX_ENABLE_THEME_SUPPORT, SETTING_ENABLE_THEME_SUPPORT);
    SetSettingForLvItemCheck(listViewHandle, PHP_OPTIONS_INDEX_ENABLE_START_ASADMIN, SETTING_ENABLE_START_AS_ADMIN);
    SetSettingForLvItemCheckRestartRequired(listViewHandle, PHP_OPTIONS_INDEX_ENABLE_STREAM_MODE, SETTING_ENABLE_STREAMER_MODE);
    SetSettingForLvItemCheck(listViewHandle, PHP_OPTIONS_INDEX_ENABLE_MONOSPACE, SETTING_ENABLE_MONOSPACE_FONT);
    //SetSettingForLvItemCheckRestartRequired(listViewHandle, PHP_OPTIONS_INDEX_ENABLE_LINUX_SUPPORT, SETTING_ENABLE_LINUX_SUBSYSTEM_SUPPORT);
    SetSettingForLvItemCheckRestartRequired(listViewHandle, PHP_OPTIONS_INDEX_ENABLE_NETWORK_RESOLVE, SETTING_ENABLE_NETWORK_RESOLVE);
    SetSettingForLvItemCheckRestartRequired(listViewHandle, PHP_OPTIONS_INDEX_ENABLE_NETWORK_RESOLVE_DOH, SETTING_ENABLE_NETWORK_RESOLVE_DOH);
    SetSettingForLvItemCheck(listViewHandle, PHP_OPTIONS_INDEX_ENABLE_INSTANT_TOOLTIPS, SETTING_ENABLE_INSTANT_TOOLTIPS);
    SetSettingForLvItemCheckRestartRequired(listViewHandle, PHP_OPTIONS_INDEX_ENABLE_IMAGE_COHERENCY, SETTING_ENABLE_IMAGE_COHERENCY_SUPPORT);
    SetSettingForLvItemCheckRestartRequired(listViewHandle, PHP_OPTIONS_INDEX_ENABLE_STAGE2, SETTING_ENABLE_STAGE2);
    SetSettingForLvItemCheckRestartRequired(listViewHandle, PHP_OPTIONS_INDEX_ENABLE_SERVICE_STAGE2, SETTING_ENABLE_SERVICE_STAGE2);
    SetSettingForLvItemCheck(listViewHandle, PHP_OPTIONS_INDEX_ICON_SINGLE_CLICK, SETTING_ICON_SINGLE_CLICK);
    SetSettingForLvItemCheck(listViewHandle, PHP_OPTIONS_INDEX_ICON_TOGGLE_VISIBILITY, SETTING_ICON_TOGGLES_VISIBILITY);
    SetSettingForLvItemCheck(listViewHandle, PHP_OPTIONS_INDEX_PROPAGATE_CPU_USAGE, SETTING_PROPAGATE_CPU_USAGE);
    SetSettingForLvItemCheck(listViewHandle, PHP_OPTIONS_INDEX_SHOW_ADVANCED_OPTIONS, SETTING_ENABLE_ADVANCED_OPTIONS);
    if (WindowsVersion >= WINDOWS_10)
        SetSettingForLvItemCheckRestartRequired(listViewHandle, PHP_OPTIONS_INDEX_PROCESS_MONITOR, SETTING_ENABLE_PROCESS_MONITOR);

    if (PhGetIntegerSetting(SETTING_ENABLE_THEME_SUPPORT)) // PhGetIntegerSetting required (dmex)
    {
        PhSetIntegerSetting(SETTING_GRAPH_COLOR_MODE, 1); // HACK switch to dark theme. (dmex)
    }
    else if (themeSupportWasEnabled)
    {
        PhSetIntegerSetting(SETTING_GRAPH_COLOR_MODE, 0); // Undo the override above when theme support is turned off.
    }

    WriteCurrentUserRun(
        ListView_GetCheckState(listViewHandle, PHP_OPTIONS_INDEX_START_ATLOGON) == BST_CHECKED,
        ListView_GetCheckState(listViewHandle, PHP_OPTIONS_INDEX_START_HIDDEN) == BST_CHECKED
        );
}

_Function_class_(USER_THREAD_START_ROUTINE)
static NTSTATUS PhpElevateAdvancedThreadStart(
    _In_ PVOID Parameter
    )
{
    PPH_STRING arguments;

    arguments = Parameter;
    PhShellProcessHacker(
        WindowHandleForElevate,
        arguments->Buffer,
        SW_SHOW,
        PH_SHELL_EXECUTE_ADMIN,
        PH_SHELL_APP_PROPAGATE_PARAMETERS,
        INFINITE,
        NULL
        );
    PhDereferenceObject(arguments);

    PostMessage(WindowHandleForElevate, WM_PH_CHILD_EXIT, 0, 0);

    return STATUS_SUCCESS;
}

UINT_PTR CALLBACK PhpChooseFontDlgHookProc(
    _In_ HWND hwndDlg,
    _In_ UINT uMsg,
    _In_ WPARAM wParam,
    _In_ LPARAM lParam
    )
{
    switch (uMsg)
    {
    case WM_INITDIALOG:
        {
            PhCenterWindow(hwndDlg, PhOptionsWindowHandle);

            PhInitializeWindowTheme(hwndDlg, PhEnableThemeSupport);
        }
        break;
    case WM_CTLCOLORBTN:
        return HANDLE_WM_CTLCOLORBTN(hwndDlg, wParam, lParam, PhWindowThemeControlColor);
    case WM_CTLCOLORDLG:
        return HANDLE_WM_CTLCOLORDLG(hwndDlg, wParam, lParam, PhWindowThemeControlColor);
    case WM_CTLCOLORSTATIC:
        return HANDLE_WM_CTLCOLORSTATIC(hwndDlg, wParam, lParam, PhWindowThemeControlColor);
    }

    return FALSE;
}

INT_PTR CALLBACK PhpOptionsGeneralDlgProc(
    _In_ HWND hwndDlg,
    _In_ UINT uMsg,
    _In_ WPARAM wParam,
    _In_ LPARAM lParam
    )
{
    static PH_LAYOUT_MANAGER LayoutManager;
    static HWND ListViewHandle = NULL;

    switch (uMsg)
    {
    case WM_INITDIALOG:
        {
            HWND comboBoxHandle;
            ULONG i;
            LOGFONT font;

            comboBoxHandle = GetDlgItem(hwndDlg, IDC_MAXSIZEUNIT);
            ListViewHandle = GetDlgItem(hwndDlg, IDC_SETTINGS);

            PhpOptionsSetImageList(ListViewHandle, FALSE);

            PhInitializeLayoutManager(&LayoutManager, hwndDlg);
            PhAddLayoutItem(&LayoutManager, GetDlgItem(hwndDlg, IDC_SEARCHENGINE), NULL, PH_ANCHOR_LEFT | PH_ANCHOR_TOP | PH_ANCHOR_RIGHT);
            PhAddLayoutItem(&LayoutManager, GetDlgItem(hwndDlg, IDC_PEVIEWER), NULL, PH_ANCHOR_LEFT | PH_ANCHOR_TOP | PH_ANCHOR_RIGHT);
            PhAddLayoutItem(&LayoutManager, ListViewHandle, NULL, PH_ANCHOR_ALL);
            PhAddLayoutItem(&LayoutManager, GetDlgItem(hwndDlg, IDC_DBGHELPSEARCHPATH), NULL, PH_ANCHOR_LEFT | PH_ANCHOR_TOP | PH_ANCHOR_RIGHT);

            PhSetListViewStyle(ListViewHandle, FALSE, TRUE);
            ListView_SetExtendedListViewStyleEx(ListViewHandle, LVS_EX_CHECKBOXES, LVS_EX_CHECKBOXES);
            PhSetControlTheme(ListViewHandle, L"explorer");
            PhAddListViewColumn(ListViewHandle, 0, 0, 0, LVCFMT_LEFT, 250, L"Nombre");
            PhSetExtendedListView(ListViewHandle);

            for (i = 0; i < RTL_NUMBER_OF(PhSizeUnitNames); i++)
                ComboBox_AddString(comboBoxHandle, PhSizeUnitNames[i]);

            if (PhMaxSizeUnit != ULONG_MAX)
                ComboBox_SetCurSel(comboBoxHandle, PhMaxSizeUnit);
            else
                ComboBox_SetCurSel(comboBoxHandle, RTL_NUMBER_OF(PhSizeUnitNames) - 1);

            PhSetDialogItemText(hwndDlg, IDC_SEARCHENGINE, PhaGetStringSetting(SETTING_SEARCH_ENGINE)->Buffer);
            PhSetDialogItemText(hwndDlg, IDC_PEVIEWER, PhaGetStringSetting(SETTING_PROGRAM_INSPECT_EXECUTABLES)->Buffer);
            PhSetDialogItemValue(hwndDlg, IDC_ICONPROCESSES, PhGetIntegerSetting(SETTING_ICON_PROCESSES), FALSE);
            PhSetDialogItemText(hwndDlg, IDC_DBGHELPSEARCHPATH, PhaGetStringSetting(SETTING_DBGHELP_SEARCH_PATH)->Buffer);

            ReadCurrentUserRun();

            // Set the font of the button for a nice preview.
            if (GetCurrentFont(&font))
            {
                CurrentFontInstance = CreateFontIndirect(&font);

                if (CurrentFontInstance)
                {
                    SetWindowFont(OptionsTreeControl, CurrentFontInstance, TRUE); // HACK
                    SetWindowFont(ListViewHandle, CurrentFontInstance, TRUE);
                    SetWindowFont(GetDlgItem(hwndDlg, IDC_FONT), CurrentFontInstance, TRUE);
                }
            }

            if (GetCurrentFontMonospace(&font))
            {
                CurrentFontMonospaceInstance = CreateFontIndirect(&font);

                if (CurrentFontMonospaceInstance)
                {
                    SetWindowFont(GetDlgItem(hwndDlg, IDC_FONTMONOSPACE), CurrentFontMonospaceInstance, TRUE);
                }
            }

            GeneralListViewStateInitializing = TRUE;
            PhpAdvancedPageLoad(hwndDlg, FALSE);
            PhpRefreshTaskManagerState(hwndDlg);
            GeneralListViewStateInitializing = FALSE;
        }
        break;
    case WM_DESTROY:
        {
            if (NewFontSelection)
            {
                PhSetStringSetting2(SETTING_FONT, &NewFontSelection->sr);
            }

            if (NewFontMonospaceSelection)
            {
                PhSetStringSetting2(SETTING_FONT_MONOSPACE, &NewFontMonospaceSelection->sr);
            }

            if (NewFontSelection || NewFontMonospaceSelection)
            {
                SystemInformer_UpdateFont();
            }

            if (CurrentFontInstance)
                DeleteFont(CurrentFontInstance);

            if (CurrentFontMonospaceInstance)
                DeleteFont(CurrentFontMonospaceInstance);

            PhClearReference(&NewFontSelection);
            PhClearReference(&NewFontMonospaceSelection);
            PhClearReference(&OldTaskMgrDebugger);

            PhDeleteLayoutManager(&LayoutManager);
        }
        break;
    case WM_DPICHANGED_AFTERPARENT:
        {
            LOGFONT font;
            HFONT fontHandle;

            if (GetCurrentFont(&font) && (fontHandle = CreateFontIndirect(&font)))
            {
                PhSwapReferenceFont(&CurrentFontInstance, NULL, fontHandle, FALSE);
                SetWindowFont(OptionsTreeControl, CurrentFontInstance, TRUE); // HACK
                SetWindowFont(ListViewHandle, CurrentFontInstance, TRUE);
                SetWindowFont(GetDlgItem(hwndDlg, IDC_FONT), CurrentFontInstance, TRUE);
            }

            if (GetCurrentFontMonospace(&font) && (fontHandle = CreateFontIndirect(&font)))
            {
                PhSwapReferenceFont(&CurrentFontMonospaceInstance, NULL, fontHandle, FALSE);
                SetWindowFont(GetDlgItem(hwndDlg, IDC_FONTMONOSPACE), CurrentFontMonospaceInstance, TRUE);
            }

            PhLayoutManagerUpdate(&LayoutManager, LOWORD(wParam));
            PhLayoutManagerLayout(&LayoutManager);

            PhpOptionsSetImageList(ListViewHandle, FALSE);
        }
        break;
    case WM_COMMAND:
        {
            switch (GET_WM_COMMAND_ID(wParam, lParam))
            {
            case IDC_FONT:
                {
                    LOGFONT font;
                    CHOOSEFONT chooseFont;

                    if (!GetCurrentFont(&font))
                    {
                        // Can't get LOGFONT from the existing setting, probably
                        // because the user hasn't ever chosen a font before.
                        // Set the font to something familiar.
                        GetObject(SystemInformer_GetFont(), sizeof(LOGFONT), &font);
                    }

                    memset(&chooseFont, 0, sizeof(CHOOSEFONT));
                    chooseFont.lStructSize = sizeof(CHOOSEFONT);
                    chooseFont.hwndOwner = hwndDlg;
                    chooseFont.lpfnHook = PhpChooseFontDlgHookProc;
                    chooseFont.lpLogFont = &font;
                    chooseFont.Flags = CF_FORCEFONTEXIST | CF_INITTOLOGFONTSTRUCT | CF_ENABLEHOOK | CF_SCREENFONTS;

                    if (ChooseFont(&chooseFont))
                    {
                        PhMoveReference(&NewFontSelection, PhBufferToHexString((PUCHAR)&font, sizeof(LOGFONT)));

                        // Update the button's font.
                        if (CurrentFontInstance) DeleteFont(CurrentFontInstance);
                        CurrentFontInstance = CreateFontIndirect(&font);

                        SetWindowFont(OptionsTreeControl, CurrentFontInstance, TRUE); // HACK
                        SetWindowFont(GetDlgItem(hwndDlg, IDC_SETTINGS), CurrentFontInstance, TRUE);
                        SetWindowFont(GetDlgItem(hwndDlg, IDC_FONT), CurrentFontInstance, TRUE);

                        // Re-add the listview items for the new font (dmex)
                        GeneralListViewStateInitializing = TRUE;
                        ExtendedListView_SetRedraw(ListViewHandle, FALSE);
                        ListView_DeleteAllItems(ListViewHandle);
                        PhpAdvancedPageLoad(hwndDlg, FALSE);
                        ExtendedListView_SetRedraw(ListViewHandle, TRUE);
                        GeneralListViewStateInitializing = FALSE;

                        RestartRequired = TRUE; // HACK: Fix ToolStatus plugin toolbar resize on font change
                    }
                }
                break;
            case IDC_FONTMONOSPACE:
                {
                    LOGFONT font;
                    CHOOSEFONT chooseFont;

                    if (!GetCurrentFontMonospace(&font))
                    {
                        // Can't get LOGFONT from the existing setting, probably
                        // because the user hasn't ever chosen a font before.
                        // Set the font to something familiar.
                        //GetObject(SystemInformer_GetFont(), sizeof(LOGFONT), &font);
                        GetObject(PhMonospaceFont, sizeof(LOGFONT), &font);
                    }

                    memset(&chooseFont, 0, sizeof(CHOOSEFONT));
                    chooseFont.lStructSize = sizeof(CHOOSEFONT);
                    chooseFont.hwndOwner = hwndDlg;
                    chooseFont.lpfnHook = PhpChooseFontDlgHookProc;
                    chooseFont.lpLogFont = &font;
                    chooseFont.Flags = CF_FORCEFONTEXIST | CF_INITTOLOGFONTSTRUCT | CF_ENABLEHOOK | CF_SCREENFONTS | CF_FIXEDPITCHONLY;

                    if (ChooseFont(&chooseFont))
                    {
                        PhMoveReference(&NewFontMonospaceSelection, PhBufferToHexString((PUCHAR)&font, sizeof(LOGFONT)));

                        // Update the button's font.
                        if (CurrentFontMonospaceInstance) DeleteFont(CurrentFontMonospaceInstance);
                        CurrentFontMonospaceInstance = CreateFontIndirect(&font);

                        SetWindowFont(GetDlgItem(hwndDlg, IDC_FONTMONOSPACE), CurrentFontMonospaceInstance, TRUE);

                        // Re-add the listview items for the new font (dmex)
                        GeneralListViewStateInitializing = TRUE;
                        ExtendedListView_SetRedraw(ListViewHandle, FALSE);
                        ListView_DeleteAllItems(ListViewHandle);
                        PhpAdvancedPageLoad(hwndDlg, FALSE);
                        ExtendedListView_SetRedraw(ListViewHandle, TRUE);
                        GeneralListViewStateInitializing = FALSE;

                        RestartRequired = TRUE; // HACK: Fix ToolStatus plugin toolbar resize on font change
                    }
                }
                break;
            case IDC_REPLACETASKMANAGER:
                {
                    WindowHandleForElevate = hwndDlg;

                    PhCreateThread2(PhpElevateAdvancedThreadStart, PhFormatString(
                        L"-showoptions -hwnd %Ix",
                        (ULONG_PTR)PhOptionsWindowHandle // GetParent(GetParent(hwndDlg))
                        ));
                }
                break;
            case IDC_SAMPLECOUNTAUTOMATIC:
                {
                    EnableWindow(GetDlgItem(hwndDlg, IDC_SAMPLECOUNT), Button_GetCheck(GetDlgItem(hwndDlg, IDC_SAMPLECOUNTAUTOMATIC)) != BST_CHECKED);
                }
                break;
            }
        }
        break;
    case WM_PH_CHILD_EXIT:
        {
            PhpRefreshTaskManagerState(hwndDlg);
        }
        break;
    case WM_SIZE:
        {
            PhLayoutManagerLayout(&LayoutManager);
        }
        break;
    case WM_NOTIFY:
        {
            LPNMHDR header = (LPNMHDR)lParam;

            switch (header->code)
            {
            case NM_CLICK:
            case NM_DBLCLK:
                {
                    LPNMITEMACTIVATE itemActivate = (LPNMITEMACTIVATE)header;
                    LVHITTESTINFO lvHitInfo;

                    lvHitInfo.pt = itemActivate->ptAction;

                    if (ListView_HitTest(ListViewHandle, &lvHitInfo) != INT_ERROR)
                    {
                        // Ignore click notifications for the listview checkbox region.
                        if (!(lvHitInfo.flags & LVHT_ONITEMSTATEICON))
                        {
                            BOOLEAN itemChecked;

                            // Emulate the checkbox control label click behavior and check/uncheck the checkbox when the listview item is clicked.
                            itemChecked = ListView_GetCheckState(ListViewHandle, itemActivate->iItem) == BST_CHECKED;
                            ListView_SetCheckState(ListViewHandle, itemActivate->iItem, !itemChecked);
                        }
                    }
                }
                break;
            case LVN_ITEMCHANGING:
                {
                    LPNM_LISTVIEW listView = (LPNM_LISTVIEW)lParam;

                    if (listView->uChanged & LVIF_STATE)
                    {
                        if (GeneralListViewStateInitializing)
                            break;

                        switch (listView->uNewState & LVIS_STATEIMAGEMASK)
                        {
                        case INDEXTOSTATEIMAGEMASK(2): // checked
                            {
                                switch (listView->iItem)
                                {
                                case PHP_OPTIONS_INDEX_HIDE_WHENCLOSED:
                                case PHP_OPTIONS_INDEX_HIDE_WHENMINIMIZED:
                                case PHP_OPTIONS_INDEX_START_HIDDEN:
                                    {
                                        if (!PhNfIconsEnabled())
                                        {
                                            PhShowInformation2(
                                                PhOptionsWindowHandle,
                                                L"No se puede configurar esta opción.",
                                                L"%s",
                                                L"Debe habilitar al menos un icono de bandeja (menú Ver > Iconos de bandeja) antes de habilitar la opción de ocultar."
                                                );
                                            SetWindowLongPtr(hwndDlg, DWLP_MSGRESULT, TRUE);
                                            return TRUE;
                                        }
                                    }
                                    break;
                                case PHP_OPTIONS_INDEX_ENABLE_START_ASADMIN:
                                    {
                                        PPH_STRING applicationFileName;

                                        if (!PhGetOwnTokenAttributes().Elevated)
                                        {
                                            PhShowInformation2(
                                                PhOptionsWindowHandle,
                                                L"No se puede habilitar la opción de iniciar como administrador.",
                                                L"%s",
                                                L"Debe habilitar esta opción con privilegios administrativos."
                                                );

                                            SetWindowLongPtr(hwndDlg, DWLP_MSGRESULT, TRUE);
                                            return TRUE;
                                        }

                                        if (applicationFileName = PhGetApplicationFileNameWin32())
                                        {
                                            static const PH_STRINGREF separator = PH_STRINGREF_INIT(L"\"");
                                            HRESULT status;
                                            PPH_STRING quotedFileName;

                                            if (!PhShowOptionsDefaultInstallLocation(PhOptionsWindowHandle, L"Habilitar la opción 'iniciar como administrador'"))
                                            {
                                                SetWindowLongPtr(hwndDlg, DWLP_MSGRESULT, TRUE);
                                                return TRUE;
                                            }

                                            quotedFileName = PH_AUTO(PhConcatStringRef3(
                                                &separator,
                                                &applicationFileName->sr,
                                                &separator
                                                ));

                                            status = PhCreateAdminTask(
                                                &SI_RUNAS_ADMIN_TASK_NAME,
                                                &quotedFileName->sr
                                                );

                                            if (FAILED(status))
                                            {
                                                PhShowStatus(
                                                    PhOptionsWindowHandle,
                                                    L"No se puede habilitar el inicio como administrador.",
                                                    0,
                                                    HRESULT_CODE(status)
                                                    );

                                                PhDereferenceObject(applicationFileName);
                                                SetWindowLongPtr(hwndDlg, DWLP_MSGRESULT, TRUE);
                                                return TRUE;
                                            }

                                            PhDereferenceObject(applicationFileName);
                                        }
                                    }
                                    break;
                                }
                            }
                            break;
                        }
                    }
                }
                break;
            case LVN_ITEMCHANGED:
                {
                    LPNM_LISTVIEW listView = (LPNM_LISTVIEW)lParam;

                    if (listView->uChanged & LVIF_STATE)
                    {
                        if (GeneralListViewStateInitializing)
                            break;

                        switch (listView->uNewState & LVIS_STATEIMAGEMASK)
                        {
                        case INDEXTOSTATEIMAGEMASK(2): // checked
                            {
                                switch (listView->iItem)
                                {
                                case PHP_OPTIONS_INDEX_SHOW_ADVANCED_OPTIONS:
                                    {
                                        PhSetIntegerSetting(SETTING_ENABLE_ADVANCED_OPTIONS, TRUE);
                                        PhpOptionsShowHideTreeViewItem(FALSE);
                                    }
                                    break;
                                case PHP_OPTIONS_INDEX_ENABLE_START_ASADMIN:
                                    break;
                                }
                            }
                            break;
                        case INDEXTOSTATEIMAGEMASK(1): // unchecked
                            {
                                switch (listView->iItem)
                                {
                                case PHP_OPTIONS_INDEX_SHOW_ADVANCED_OPTIONS:
                                    {
                                        PhSetIntegerSetting(SETTING_ENABLE_ADVANCED_OPTIONS, FALSE);
                                        PhpOptionsShowHideTreeViewItem(TRUE);
                                    }
                                    break;
                                case PHP_OPTIONS_INDEX_ENABLE_START_ASADMIN:
                                    {
                                        if (PhGetIntegerSetting(SETTING_ENABLE_START_AS_ADMIN))
                                        {
                                            PhDeleteAdminTask(&SI_RUNAS_ADMIN_TASK_NAME);
                                        }
                                    }
                                    break;
                                }
                            }
                            break;
                        }
                    }
                }
                break;
            }
        }
        break;
    //case WM_CTLCOLORSTATIC:
    //    {
    //        static HFONT BoldFont = NULL; // leak
    //        HWND control = (HWND)lParam;
    //        HDC hdc = (HDC)wParam;
    //
    //        if (GetDlgCtrlID(control) == IDC_DEFSTATE)
    //        {
    //            if (!BoldFont)
    //                BoldFont = PhDuplicateFontWithNewWeight(GetWindowFont(control), FW_BOLD);
    //
    //            SetBkMode(hdc, TRANSPARENT);
    //
    //            if (!PhpIsDefaultTaskManager())
    //            {
    //                SelectFont(hdc, BoldFont);
    //            }
    //        }
    //    }
    //    break;
    case WM_CTLCOLORBTN:
        return HANDLE_WM_CTLCOLORBTN(hwndDlg, wParam, lParam, PhWindowThemeControlColor);
    case WM_CTLCOLORDLG:
        return HANDLE_WM_CTLCOLORDLG(hwndDlg, wParam, lParam, PhWindowThemeControlColor);
    case WM_CTLCOLORSTATIC:
        return HANDLE_WM_CTLCOLORSTATIC(hwndDlg, wParam, lParam, PhWindowThemeControlColor);
    }

    return FALSE;
}

// Editor parameters passed to PhpOptionsAdvancedEditDlgProc. SettingSchema (when present) is owned
// by the advanced context's schema object and remains valid for the lifetime of the modal dialog.
typedef struct _PH_OPTIONS_ADVANCED_EDIT_CONTEXT
{
    PPH_SETTING Setting;
    PPH_STRING Description;
    PVOID SettingSchema;
} PH_OPTIONS_ADVANCED_EDIT_CONTEXT, *PPH_OPTIONS_ADVANCED_EDIT_CONTEXT;

static PPH_STRING OptionsAdvancedFormatSupportedValues(
    _In_ PVOID SettingSchema,
    _In_ PH_SETTING_TYPE Type
    );

_Success_(return)
static BOOLEAN OptionsAdvancedValidateAgainstSchema(
    _In_ PVOID SettingSchema,
    _In_ PH_SETTING_TYPE Type,
    _In_ PPH_STRING Value,
    _Out_ PPH_STRING* Warning
    );

static INT_PTR CALLBACK PhpOptionsAdvancedEditDlgProc(
    _In_ HWND hwndDlg,
    _In_ UINT uMsg,
    _In_ WPARAM wParam,
    _In_ LPARAM lParam
    )
{
    static PH_LAYOUT_MANAGER LayoutManager;

    switch (uMsg)
    {
    case WM_INITDIALOG:
        {
            PPH_OPTIONS_ADVANCED_EDIT_CONTEXT editContext = (PPH_OPTIONS_ADVANCED_EDIT_CONTEXT)lParam;
            PPH_SETTING setting = editContext->Setting;

            PhSetApplicationWindowIcon(hwndDlg);

            PhSetWindowText(hwndDlg, L"Editor de ajustes");
            PhCenterWindow(hwndDlg, GetParent(hwndDlg));

            PhSetWindowContext(hwndDlg, PH_WINDOW_CONTEXT_DEFAULT, editContext);

            PhInitializeLayoutManager(&LayoutManager, hwndDlg);
            PhAddLayoutItem(&LayoutManager, GetDlgItem(hwndDlg, IDC_NAME), NULL, PH_ANCHOR_LEFT | PH_ANCHOR_TOP | PH_ANCHOR_RIGHT);
            PhAddLayoutItem(&LayoutManager, GetDlgItem(hwndDlg, IDC_DESCRIPTION), NULL, PH_ANCHOR_LEFT | PH_ANCHOR_TOP | PH_ANCHOR_RIGHT);
            PhAddLayoutItem(&LayoutManager, GetDlgItem(hwndDlg, IDC_SUPPORTED), NULL, PH_ANCHOR_LEFT | PH_ANCHOR_TOP | PH_ANCHOR_RIGHT);
            PhAddLayoutItem(&LayoutManager, GetDlgItem(hwndDlg, IDC_VALUE), NULL, PH_ANCHOR_ALL);
            PhAddLayoutItem(&LayoutManager, GetDlgItem(hwndDlg, IDOK), NULL, PH_ANCHOR_RIGHT | PH_ANCHOR_BOTTOM);
            PhAddLayoutItem(&LayoutManager, GetDlgItem(hwndDlg, IDCANCEL), NULL, PH_ANCHOR_RIGHT | PH_ANCHOR_BOTTOM);

            PhSetDialogItemText(hwndDlg, IDC_NAME, setting->Name.Buffer);
            PhSetDialogItemText(hwndDlg, IDC_VALUE, PH_AUTO_T(PH_STRING, PhSettingToString(setting->Type, setting))->Buffer);

            if (editContext->Description)
            {
                // Schema descriptions use lone '\n'; convert to '\r\n' for the multiline edit control.
                PH_STRING_BUILDER stringBuilder;
                SIZE_T length = editContext->Description->Length / sizeof(WCHAR);

                PhInitializeStringBuilder(&stringBuilder, editContext->Description->Length + 16);

                for (SIZE_T i = 0; i < length; i++)
                {
                    WCHAR character = editContext->Description->Buffer[i];

                    if (character == L'\n' && (i == 0 || editContext->Description->Buffer[i - 1] != L'\r'))
                        PhAppendStringBuilder2(&stringBuilder, L"\r\n");
                    else
                        PhAppendCharStringBuilder(&stringBuilder, character);
                }

                PhSetDialogItemText(hwndDlg, IDC_DESCRIPTION, stringBuilder.String->Buffer);
                PhDeleteStringBuilder(&stringBuilder);
            }
            else
            {
                PhSetDialogItemText(hwndDlg, IDC_DESCRIPTION, L"No hay descripción de esquema disponible.");
            }

            if (editContext->SettingSchema)
            {
                PPH_STRING supported = OptionsAdvancedFormatSupportedValues(editContext->SettingSchema, setting->Type);
                PhSetDialogItemText(hwndDlg, IDC_SUPPORTED, supported->Buffer);
                PhDereferenceObject(supported);
            }

            EnableWindow(GetDlgItem(hwndDlg, IDC_NAME), FALSE);

            PhSetDialogFocus(hwndDlg, GetDlgItem(hwndDlg, IDCANCEL));

            PhInitializeWindowTheme(hwndDlg, PhEnableThemeSupport);
        }
        break;
    case WM_DESTROY:
        {
            PhRemoveWindowContext(hwndDlg, PH_WINDOW_CONTEXT_DEFAULT);

            PhDeleteLayoutManager(&LayoutManager);
        }
        break;
    case WM_DPICHANGED_AFTERPARENT:
        {
            PhLayoutManagerUpdate(&LayoutManager, PhGetWindowDpi(hwndDlg));
            PhLayoutManagerLayout(&LayoutManager);
        }
        break;
    case WM_SIZE:
        {
            PhLayoutManagerLayout(&LayoutManager);
        }
        break;
    case WM_COMMAND:
        {
            switch (GET_WM_COMMAND_ID(wParam, lParam))
            {
            case IDCANCEL:
                EndDialog(hwndDlg, IDCANCEL);
                break;
            case IDOK:
                {
                    PPH_OPTIONS_ADVANCED_EDIT_CONTEXT editContext = PhGetWindowContext(hwndDlg, PH_WINDOW_CONTEXT_DEFAULT);
                    PPH_SETTING setting = editContext->Setting;
                    PPH_STRING settingValue = PH_AUTO(PhGetWindowText(GetDlgItem(hwndDlg, IDC_VALUE)));

                    // Schema validation is advisory: warn on a constraint violation but still apply the value.
                    if (editContext->SettingSchema)
                    {
                        PPH_STRING warning;

                        if (!OptionsAdvancedValidateAgainstSchema(editContext->SettingSchema, setting->Type, settingValue, &warning))
                        {
                            PhShowWarning2(
                                hwndDlg,
                                L"El valor está fuera de los valores admitidos por el esquema.",
                                L"\"%s\" is not one of the supported values (%s).\r\nThe value was applied anyway.",
                                settingValue->Buffer,
                                PhGetString(warning)
                                );
                            PhClearReference(&warning);
                        }
                    }

                    if (!PhSettingFromString(
                        setting->Type,
                        &settingValue->sr,
                        settingValue,
                        setting
                        ))
                    {
                        PhSettingFromString(
                            setting->Type,
                            &setting->DefaultValue,
                            NULL,
                            setting
                            );
                    }

                    PhReloadGeneralSection();

                    EndDialog(hwndDlg, IDOK);
                }
                break;
            }
        }
        break;
    case WM_CTLCOLORBTN:
        return HANDLE_WM_CTLCOLORBTN(hwndDlg, wParam, lParam, PhWindowThemeControlColor);
    case WM_CTLCOLORDLG:
        return HANDLE_WM_CTLCOLORDLG(hwndDlg, wParam, lParam, PhWindowThemeControlColor);
    case WM_CTLCOLORSTATIC:
        return HANDLE_WM_CTLCOLORSTATIC(hwndDlg, wParam, lParam, PhWindowThemeControlColor);
    }

    return FALSE;
}

#pragma region Plugin TreeList

#define WM_PH_OPTIONS_ADVANCED (WM_APP + 451)

typedef struct _PH_OPTIONS_ADVANCED_CONTEXT
{
    PH_LAYOUT_MANAGER LayoutManager;

    HWND WindowHandle;
    HWND TreeNewHandle;
    HWND SearchBoxHandle;

    union
    {
        ULONG Flags;
        struct
        {
            ULONG HideModified : 1;
            ULONG HideDefault : 1;
            ULONG HighlightModified : 1;
            ULONG HighlightDefault : 1;
            ULONG Spare : 28;
        };
    };

    ULONG TreeNewSortColumn;
    PH_SORT_ORDER TreeNewSortOrder;
    PH_TN_FILTER_SUPPORT TreeFilterSupport;
    PPH_TN_FILTER_ENTRY TreeFilterEntry;
    PPH_HASHTABLE NodeHashtable;
    PPH_LIST NodeList;
    ULONG_PTR SearchMatchHandle;

    PVOID SchemaObject;     // Root settings.schema.json object (owns the tree, freed on delete).
    PVOID SchemaProperties; // The "properties" object inside SchemaObject (not owned).
} PH_OPTIONS_ADVANCED_CONTEXT, *PPH_OPTIONS_ADVANCED_CONTEXT;

typedef enum _PH_OPTIONS_ADVANCED_TREE_ITEM_MENU
{
    PH_OPTIONS_ADVANCED_TREE_ITEM_MENU_HIDE_MODIFIED = 1,
    PH_OPTIONS_ADVANCED_TREE_ITEM_MENU_HIDE_DEFAULT,
    PH_OPTIONS_ADVANCED_TREE_ITEM_MENU_HIGHLIGHT_MODIFIED,
    PH_OPTIONS_ADVANCED_TREE_ITEM_MENU_HIGHLIGHT_DEFAULT,
} PH_OPTIONS_ADVANCED_ITEM_MENU;

typedef enum _PH_OPTIONS_ADVANCED_COLUMN_ITEM
{
    PH_OPTIONS_ADVANCED_COLUMN_ITEM_NAME,
    PH_OPTIONS_ADVANCED_COLUMN_ITEM_TYPE,
    PH_OPTIONS_ADVANCED_COLUMN_ITEM_VALUE,
    PH_OPTIONS_ADVANCED_COLUMN_ITEM_DEFAULT,
    PH_OPTIONS_ADVANCED_COLUMN_ITEM_DESCRIPTION,
    PH_OPTIONS_ADVANCED_COLUMN_ITEM_MAXIMUM
} PH_OPTIONS_ADVANCED_COLUMN_ITEM;

typedef struct _PH_OPTIONS_ADVANCED_ROOT_NODE
{
    PH_TREENEW_NODE Node;

    PH_SETTING_TYPE Type;
    PPH_SETTING Setting;
    PPH_STRING Name;
    PPH_STRING ValueString;
    PPH_STRING DefaultString;
    PPH_STRING Description;     // Schema description, or NULL when no schema is loaded.
    PVOID SettingSchema;       // Per-setting schema object (owned by Context->SchemaObject).

    PH_STRINGREF TextCache[PH_OPTIONS_ADVANCED_COLUMN_ITEM_MAXIMUM];
} PH_OPTIONS_ADVANCED_ROOT_NODE, *PPH_OPTIONS_ADVANCED_ROOT_NODE;


#define SORT_FUNCTION(Column) OptionsAdvancedTreeNewCompare##Column
#define BEGIN_SORT_FUNCTION(Column) static long __cdecl OptionsAdvancedTreeNewCompare##Column( \
    _In_ void *_context, \
    _In_ const void *_elem1, \
    _In_ const void *_elem2 \
    ) \
{ \
    PPH_OPTIONS_ADVANCED_ROOT_NODE node1 = *(PPH_OPTIONS_ADVANCED_ROOT_NODE*)_elem1; \
    PPH_OPTIONS_ADVANCED_ROOT_NODE node2 = *(PPH_OPTIONS_ADVANCED_ROOT_NODE*)_elem2; \
    LONG sortResult = 0;

#define END_SORT_FUNCTION \
    if (sortResult == 0) \
        sortResult = uintptrcmp((ULONG_PTR)node1->Node.Index, (ULONG_PTR)node2->Node.Index); \
    \
    return PhModifySort(sortResult, ((PPH_OPTIONS_ADVANCED_CONTEXT)_context)->TreeNewSortOrder); \
}

BEGIN_SORT_FUNCTION(Name)
{
    sortResult = PhCompareString(node1->Name, node2->Name, TRUE);
}
END_SORT_FUNCTION

BEGIN_SORT_FUNCTION(Type)
{
    sortResult = uintcmp(node1->Type, node2->Type);
}
END_SORT_FUNCTION

BEGIN_SORT_FUNCTION(Value)
{
    sortResult = PhCompareString(node1->ValueString, node2->ValueString, TRUE);
}
END_SORT_FUNCTION

BEGIN_SORT_FUNCTION(Default)
{
    sortResult = PhCompareString(node1->DefaultString, node2->DefaultString, TRUE);
}
END_SORT_FUNCTION

BEGIN_SORT_FUNCTION(Description)
{
    sortResult = PhCompareStringWithNull(node1->Description, node2->Description, TRUE);
}
END_SORT_FUNCTION

// settings.schema.json support: load the schema describing every setting and use it to surface
// descriptions, supported values and value validation in the advanced options editor.

static VOID OptionsAdvancedLoadSchema(
    _Inout_ PPH_OPTIONS_ADVANCED_CONTEXT Context
    )
{
    PPH_STRING fileName;
    PVOID object;

    if (fileName = PhGetApplicationDirectoryFileNameZ(L"resources\\settings.schema.json", TRUE))
    {
        if (NT_SUCCESS(PhLoadJsonObjectFromFile(&object, &fileName->sr)))
        {
            Context->SchemaObject = object;
            Context->SchemaProperties = PhGetJsonObject(object, "properties");
        }

        PhDereferenceObject(fileName);
    }
}

static VOID OptionsAdvancedFreeSchema(
    _Inout_ PPH_OPTIONS_ADVANCED_CONTEXT Context
    )
{
    if (Context->SchemaObject)
    {
        PhFreeJsonObject(Context->SchemaObject);
        Context->SchemaObject = NULL;
        Context->SchemaProperties = NULL;
    }
}

// Returns the per-setting schema object (owned by Context->SchemaObject), or NULL.
static PVOID OptionsAdvancedGetSettingSchema(
    _In_ PPH_OPTIONS_ADVANCED_CONTEXT Context,
    _In_ PCPH_STRINGREF Name
    )
{
    PVOID object;
    PPH_BYTES name;

    if (!Context->SchemaProperties)
        return NULL;

    name = PhConvertUtf16ToUtf8Ex(Name->Buffer, Name->Length);
    object = PhGetJsonObject(Context->SchemaProperties, name->Buffer);
    PhDereferenceObject(name);

    return object;
}

// Finds a constraint node (e.g. "enum", "minimum", "maximum") either at the top level of the
// setting schema or inside one of its "anyOf" branches.
static PVOID OptionsAdvancedFindSchemaNode(
    _In_ PVOID SettingSchema,
    _In_ PCSTR Key
    )
{
    PVOID node;
    PVOID anyOf;

    if (node = PhGetJsonObject(SettingSchema, Key))
        return node;

    if (anyOf = PhGetJsonObject(SettingSchema, "anyOf"))
    {
        ULONG count = PhGetJsonArrayLength(anyOf);

        for (ULONG i = 0; i < count; i++)
        {
            PVOID entry;

            if ((entry = PhGetJsonArrayIndexObject(anyOf, i)) && (node = PhGetJsonObject(entry, Key)))
                return node;
        }
    }

    return NULL;
}

// Builds a human-readable supported-values hint. Schema constraints are expressed in decimal;
// integer settings are entered/displayed in hexadecimal, so integer constraints are converted.
static PPH_STRING OptionsAdvancedFormatSupportedValues(
    _In_ PVOID SettingSchema,
    _In_ PH_SETTING_TYPE Type
    )
{
    PH_STRING_BUILDER stringBuilder;
    PVOID enumObject;
    PVOID minimumNode;
    PVOID maximumNode;

    PhInitializeStringBuilder(&stringBuilder, 64);

    enumObject = OptionsAdvancedFindSchemaNode(SettingSchema, "enum");
    minimumNode = OptionsAdvancedFindSchemaNode(SettingSchema, "minimum");
    maximumNode = OptionsAdvancedFindSchemaNode(SettingSchema, "maximum");

    if (enumObject && PhGetJsonObjectType(enumObject) == PH_JSON_OBJECT_TYPE_ARRAY)
    {
        ULONG count = PhGetJsonArrayLength(enumObject);
        BOOLEAN first = TRUE;

        if (Type == IntegerSettingType)
            PhAppendStringBuilder2(&stringBuilder, L"(hex) ");

        for (ULONG i = 0; i < count; i++)
        {
            PVOID element;
            PPH_STRING value;

            if (!(element = PhGetJsonArrayIndexObject(enumObject, i)))
                continue;

            value = PhGetJsonObjectString(element);

            if (!first)
                PhAppendStringBuilder2(&stringBuilder, L", ");
            first = FALSE;

            if (Type == IntegerSettingType)
            {
                ULONG64 integer;

                if (PhStringToInteger64(&value->sr, 10, &integer))
                    PhAppendFormatStringBuilder(&stringBuilder, L"%I64x", integer);
                else
                    PhAppendStringBuilder(&stringBuilder, &value->sr);
            }
            else
            {
                PhAppendStringBuilder(&stringBuilder, &value->sr);
            }

            PhDereferenceObject(value);
        }
    }
    else if (minimumNode || maximumNode)
    {
        ULONG64 minimum = minimumNode ? (ULONG64)PhGetJsonInt64Object(minimumNode) : 0;
        ULONG64 maximum = maximumNode ? (ULONG64)PhGetJsonInt64Object(maximumNode) : 0;

        if (Type == IntegerSettingType)
            PhAppendFormatStringBuilder(&stringBuilder, L"(hex) %I64x - %I64x", minimum, maximum);
        else
            PhAppendFormatStringBuilder(&stringBuilder, L"%I64u - %I64u", minimum, maximum);
    }
    else
    {
        switch (Type)
        {
        case StringSettingType:
            PhAppendStringBuilder2(&stringBuilder, L"String");
            break;
        case IntegerSettingType:
            PhAppendStringBuilder2(&stringBuilder, L"Entero (hex)");
            break;
        case IntegerPairSettingType:
            PhAppendStringBuilder2(&stringBuilder, L"x,y");
            break;
        case ScalableIntegerPairSettingType:
            PhAppendStringBuilder2(&stringBuilder, L"@dpi|x,y");
            break;
        }
    }

    return PhFinalStringBuilderString(&stringBuilder);
}

// Validates Value against the schema's enum/range constraints. Returns TRUE when the value is
// acceptable (or unconstrained); on FALSE, Warning receives the supported-values hint. Comparisons
// are numeric so that hexadecimal entry never clashes with the schema's decimal constraints.
_Success_(return)
static BOOLEAN OptionsAdvancedValidateAgainstSchema(
    _In_ PVOID SettingSchema,
    _In_ PH_SETTING_TYPE Type,
    _In_ PPH_STRING Value,
    _Out_ PPH_STRING* Warning
    )
{
    PVOID enumObject;
    PVOID minimumNode;
    PVOID maximumNode;

    *Warning = NULL;

    enumObject = OptionsAdvancedFindSchemaNode(SettingSchema, "enum");

    if (enumObject && PhGetJsonObjectType(enumObject) == PH_JSON_OBJECT_TYPE_ARRAY)
    {
        ULONG count = PhGetJsonArrayLength(enumObject);
        ULONG64 value64 = 0;
        BOOLEAN valueIsInteger;

        valueIsInteger = (Type == IntegerSettingType) && PhStringToInteger64(&Value->sr, 16, &value64);

        for (ULONG i = 0; i < count; i++)
        {
            PVOID element;
            PPH_STRING member;
            BOOLEAN match = FALSE;

            if (!(element = PhGetJsonArrayIndexObject(enumObject, i)))
                continue;

            member = PhGetJsonObjectString(element);

            if (valueIsInteger)
            {
                ULONG64 member64;

                if (PhStringToInteger64(&member->sr, 10, &member64) && member64 == value64)
                    match = TRUE;
            }
            else if (PhEqualString(member, Value, FALSE))
            {
                match = TRUE;
            }

            PhDereferenceObject(member);

            if (match)
                return TRUE;
        }

        *Warning = OptionsAdvancedFormatSupportedValues(SettingSchema, Type);
        return FALSE;
    }

    minimumNode = OptionsAdvancedFindSchemaNode(SettingSchema, "minimum");
    maximumNode = OptionsAdvancedFindSchemaNode(SettingSchema, "maximum");

    if ((minimumNode || maximumNode) && Type == IntegerSettingType)
    {
        ULONG64 value64;

        if (PhStringToInteger64(&Value->sr, 16, &value64))
        {
            ULONG64 minimum = minimumNode ? (ULONG64)PhGetJsonInt64Object(minimumNode) : 0;
            ULONG64 maximum = maximumNode ? (ULONG64)PhGetJsonInt64Object(maximumNode) : MAXULONG64;

            if (value64 < minimum || value64 > maximum)
            {
                *Warning = OptionsAdvancedFormatSupportedValues(SettingSchema, Type);
                return FALSE;
            }
        }
    }

    return TRUE;
}

VOID OptionsAdvancedLoadSettingsTreeList(
    _Inout_ PPH_OPTIONS_ADVANCED_CONTEXT Context
    )
{
    PPH_STRING settings;

    settings = PhGetStringSetting(SETTING_OPTIONS_WINDOW_ADVANCED_COLUMNS);
    Context->Flags = PhGetIntegerSetting(SETTING_OPTIONS_WINDOW_ADVANCED_FLAGS);
    PhCmLoadSettings(Context->TreeNewHandle, &settings->sr);
    PhDereferenceObject(settings);
}

VOID OptionsAdvancedSaveSettingsTreeList(
    _Inout_ PPH_OPTIONS_ADVANCED_CONTEXT Context
    )
{
    PPH_STRING settings;

    settings = PhCmSaveSettings(Context->TreeNewHandle);
    PhSetStringSetting2(SETTING_OPTIONS_WINDOW_ADVANCED_COLUMNS, &settings->sr);
    PhSetIntegerSetting(SETTING_OPTIONS_WINDOW_ADVANCED_FLAGS, Context->Flags);
    PhDereferenceObject(settings);
}

VOID OptionsAdvancedSetOptionsTreeList(
    _Inout_ PPH_OPTIONS_ADVANCED_CONTEXT Context,
    _In_ ULONG Options
    )
{
    switch (Options)
    {
    case PH_OPTIONS_ADVANCED_TREE_ITEM_MENU_HIDE_MODIFIED:
        Context->HideModified = !Context->HideModified;
        break;
    case PH_OPTIONS_ADVANCED_TREE_ITEM_MENU_HIDE_DEFAULT:
        Context->HideDefault = !Context->HideDefault;
        break;
    case PH_OPTIONS_ADVANCED_TREE_ITEM_MENU_HIGHLIGHT_MODIFIED:
        Context->HighlightModified = !Context->HighlightModified;
        break;
    case PH_OPTIONS_ADVANCED_TREE_ITEM_MENU_HIGHLIGHT_DEFAULT:
        Context->HighlightDefault = !Context->HighlightDefault;
        break;
    }
}

_Function_class_(PH_HASHTABLE_EQUAL_FUNCTION)
BOOLEAN OptionsAdvancedNodeHashtableEqualFunction(
    _In_ PVOID Entry1,
    _In_ PVOID Entry2
    )
{
    PPH_OPTIONS_ADVANCED_ROOT_NODE node1 = *(PPH_OPTIONS_ADVANCED_ROOT_NODE*)Entry1;
    PPH_OPTIONS_ADVANCED_ROOT_NODE node2 = *(PPH_OPTIONS_ADVANCED_ROOT_NODE*)Entry2;

    return PhEqualStringRef(&node1->Name->sr, &node2->Name->sr, TRUE);
}

_Function_class_(PH_HASHTABLE_HASH_FUNCTION)
ULONG OptionsAdvancedNodeHashtableHashFunction(
    _In_ PVOID Entry
    )
{
    return PhHashStringRefEx(&(*(PPH_OPTIONS_ADVANCED_ROOT_NODE*)Entry)->Name->sr, TRUE, PH_STRING_HASH_X65599);
}

VOID DestroyOptionsAdvancedNode(
    _In_ PPH_OPTIONS_ADVANCED_ROOT_NODE Node
    )
{
    PhClearReference(&Node->Name);
    PhClearReference(&Node->ValueString);
    PhClearReference(&Node->DefaultString);
    PhClearReference(&Node->Description);

    PhFree(Node);
}

PPH_OPTIONS_ADVANCED_ROOT_NODE AddOptionsAdvancedNode(
    _Inout_ PPH_OPTIONS_ADVANCED_CONTEXT Context,
    _In_ PPH_SETTING Setting
    )
{
    PPH_OPTIONS_ADVANCED_ROOT_NODE node;

    node = PhAllocate(sizeof(PH_OPTIONS_ADVANCED_ROOT_NODE));
    memset(node, 0, sizeof(PH_OPTIONS_ADVANCED_ROOT_NODE));

    PhInitializeTreeNewNode(&node->Node);

    memset(node->TextCache, 0, sizeof(PH_STRINGREF) * PH_OPTIONS_ADVANCED_COLUMN_ITEM_MAXIMUM);
    node->Node.TextCache = node->TextCache;
    node->Node.TextCacheSize = PH_OPTIONS_ADVANCED_COLUMN_ITEM_MAXIMUM;

    node->Setting = Setting;
    node->Type = Setting->Type;
    node->Name = PhCreateString2(&Setting->Name);
    node->ValueString = PhSettingToString(Setting->Type, Setting);
    node->DefaultString = PhCreateString2(&Setting->DefaultValue);
    node->SettingSchema = OptionsAdvancedGetSettingSchema(Context, &Setting->Name);

    if (node->SettingSchema)
        node->Description = PhGetJsonValueAsString(node->SettingSchema, "description");

    PhAddEntryHashtable(Context->NodeHashtable, &node);
    PhAddItemList(Context->NodeList, node);

    if (Context->TreeFilterSupport.FilterList)
        node->Node.Visible = PhApplyTreeNewFiltersToNode(&Context->TreeFilterSupport, &node->Node);

    return node;
}

PPH_OPTIONS_ADVANCED_ROOT_NODE FindOptionsAdvancedNode(
    _In_ PPH_OPTIONS_ADVANCED_CONTEXT Context,
    _In_ PPH_STRING Name
    )
{
    PH_OPTIONS_ADVANCED_ROOT_NODE lookupNode;
    PPH_OPTIONS_ADVANCED_ROOT_NODE lookupNodePtr = &lookupNode;
    PPH_OPTIONS_ADVANCED_ROOT_NODE* foundNode;

    lookupNode.Name = Name;

    foundNode = (PPH_OPTIONS_ADVANCED_ROOT_NODE*)PhFindEntryHashtable(
        Context->NodeHashtable,
        &lookupNodePtr
        );

    if (foundNode)
        return *foundNode;
    else
        return NULL;
}

VOID RemoveOptionsAdvancedNode(
    _In_ PPH_OPTIONS_ADVANCED_CONTEXT Context,
    _In_ PPH_OPTIONS_ADVANCED_ROOT_NODE Node
    )
{
    ULONG index = 0;

    PhRemoveEntryHashtable(Context->NodeHashtable, &Node);

    if ((index = PhFindItemList(Context->NodeList, Node)) != ULONG_MAX)
    {
        PhRemoveItemList(Context->NodeList, index);
    }

    DestroyOptionsAdvancedNode(Node);
    TreeNew_NodesStructured(Context->TreeNewHandle);
}

VOID UpdateOptionsAdvancedNode(
    _In_ PPH_OPTIONS_ADVANCED_CONTEXT Context,
    _In_ PPH_OPTIONS_ADVANCED_ROOT_NODE Node
    )
{
    memset(Node->TextCache, 0, sizeof(PH_STRINGREF) * PH_OPTIONS_ADVANCED_COLUMN_ITEM_MAXIMUM);

    PhInvalidateTreeNewNode(&Node->Node, TN_CACHE_COLOR);
    TreeNew_NodesStructured(Context->TreeNewHandle);
}

BOOLEAN NTAPI OptionsAdvancedTreeNewCallback(
    _In_ HWND hwnd,
    _In_ PH_TREENEW_MESSAGE Message,
    _In_ PVOID Parameter1,
    _In_ PVOID Parameter2,
    _In_ PVOID Context
    )
{
    PPH_OPTIONS_ADVANCED_CONTEXT context = Context;
    PPH_OPTIONS_ADVANCED_ROOT_NODE node;

    switch (Message)
    {
    case TreeNewGetChildren:
        {
            PPH_TREENEW_GET_CHILDREN getChildren = Parameter1;
            node = (PPH_OPTIONS_ADVANCED_ROOT_NODE)getChildren->Node;

            if (!getChildren->Node)
            {
                static CONST _CoreCrtSecureSearchSortCompareFunction sortFunctions[] =
                {
                    SORT_FUNCTION(Name),
                    SORT_FUNCTION(Type),
                    SORT_FUNCTION(Value),
                    SORT_FUNCTION(Default),
                    SORT_FUNCTION(Description),
                };
                _CoreCrtSecureSearchSortCompareFunction sortFunction;

                if (context->TreeNewSortColumn < PH_OPTIONS_ADVANCED_COLUMN_ITEM_MAXIMUM)
                    sortFunction = sortFunctions[context->TreeNewSortColumn];
                else
                    sortFunction = NULL;

                if (sortFunction)
                {
                    qsort_s(context->NodeList->Items, context->NodeList->Count, sizeof(PVOID), sortFunction, context);
                }

                getChildren->Children = (PPH_TREENEW_NODE*)context->NodeList->Items;
                getChildren->NumberOfChildren = context->NodeList->Count;
            }
        }
        return TRUE;
    case TreeNewIsLeaf:
        {
            PPH_TREENEW_IS_LEAF isLeaf = (PPH_TREENEW_IS_LEAF)Parameter1;
            node = (PPH_OPTIONS_ADVANCED_ROOT_NODE)isLeaf->Node;

            isLeaf->IsLeaf = TRUE;
        }
        return TRUE;
    case TreeNewGetCellText:
        {
            PPH_TREENEW_GET_CELL_TEXT getCellText = (PPH_TREENEW_GET_CELL_TEXT)Parameter1;
            node = (PPH_OPTIONS_ADVANCED_ROOT_NODE)getCellText->Node;

            switch (getCellText->Id)
            {
            case PH_OPTIONS_ADVANCED_COLUMN_ITEM_NAME:
                getCellText->Text = PhGetStringRef(node->Name);
                break;
            case PH_OPTIONS_ADVANCED_COLUMN_ITEM_TYPE:
                {
                    switch (node->Type)
                    {
                    case StringSettingType:
                        PhInitializeStringRef(&getCellText->Text, L"String");
                        break;
                    case IntegerSettingType:
                        PhInitializeStringRef(&getCellText->Text, L"Entero");
                        break;
                    case IntegerPairSettingType:
                        PhInitializeStringRef(&getCellText->Text, L"IntegerPair");
                        break;
                    case ScalableIntegerPairSettingType:
                        PhInitializeStringRef(&getCellText->Text, L"ScalableIntegerPair");
                        break;
                    }
                }
                break;
            case PH_OPTIONS_ADVANCED_COLUMN_ITEM_VALUE:
                getCellText->Text = PhGetStringRef(node->ValueString);
                break;
            case PH_OPTIONS_ADVANCED_COLUMN_ITEM_DEFAULT:
                getCellText->Text = PhGetStringRef(node->DefaultString);
                break;
            case PH_OPTIONS_ADVANCED_COLUMN_ITEM_DESCRIPTION:
                getCellText->Text = PhGetStringRef(node->Description);
                break;
            default:
                return FALSE;
            }

            //getCellText->Flags = TN_CACHE;
        }
        return TRUE;
    case TreeNewGetNodeColor:
        {
            PPH_TREENEW_GET_NODE_COLOR getNodeColor = Parameter1;
            node = (PPH_OPTIONS_ADVANCED_ROOT_NODE)getNodeColor->Node;

            switch (node->Type)
            {
            case StringSettingType:
            case IntegerPairSettingType:
            case ScalableIntegerPairSettingType:
                {
                    if (PhEqualString(node->DefaultString, node->ValueString, TRUE))
                    {
                        if (context->HighlightDefault)
                        {
                            getNodeColor->BackColor = PhCsColorServiceProcesses;
                        }
                    }
                    else
                    {
                        if (context->HighlightModified)
                        {
                            getNodeColor->BackColor = PhCsColorSystemProcesses;
                        }
                    }
                }
                break;
            case IntegerSettingType:
                {
                    ULONG64 integer;

                    if (PhStringToInteger64(&node->DefaultString->sr, 16, &integer))
                    {
                        if (node->Setting->u.Integer == (ULONG)integer)
                        {
                            if (context->HighlightDefault)
                            {
                                getNodeColor->BackColor = PhCsColorServiceProcesses;
                            }
                        }
                        else
                        {
                            if (context->HighlightModified)
                            {
                                getNodeColor->BackColor = PhCsColorSystemProcesses;
                            }
                        }
                    }
                }
                break;
            }

            getNodeColor->Flags = TN_AUTO_FORECOLOR;
        }
        return TRUE;
    case TreeNewSortChanged:
        {
            PPH_TREENEW_SORT_CHANGED_EVENT sorting = Parameter1;

            context->TreeNewSortColumn = sorting->SortColumn;
            context->TreeNewSortOrder = sorting->SortOrder;

            // Force a rebuild to sort the items.
            TreeNew_NodesStructured(hwnd);
        }
        return TRUE;
    case TreeNewKeyDown:
        {
            PPH_TREENEW_KEY_EVENT keyEvent = Parameter1;

            switch (keyEvent->VirtualKey)
            {
            case 'C':
                if (GetKeyState(VK_CONTROL) < 0)
                    SendMessage(context->WindowHandle, WM_COMMAND, IDC_COPY, 0);
                break;
            }
        }
        return TRUE;
    case TreeNewLeftDoubleClick:
        {
            PPH_TREENEW_MOUSE_EVENT mouseEvent = Parameter1;

            SendMessage(context->WindowHandle, WM_COMMAND, WM_PH_OPTIONS_ADVANCED, (LPARAM)mouseEvent);
        }
        return TRUE;
    case TreeNewContextMenu:
        {
            PPH_TREENEW_CONTEXT_MENU contextMenuEvent = Parameter1;

            SendMessage(context->WindowHandle, WM_CONTEXTMENU, (WPARAM)hwnd, (LPARAM)contextMenuEvent);
        }
        return TRUE;
    case TreeNewHeaderRightClick:
        {
            PH_TN_COLUMN_MENU_DATA data;

            data.TreeNewHandle = hwnd;
            data.MouseEvent = Parameter1;
            data.DefaultSortColumn = 0;
            data.DefaultSortOrder = AscendingSortOrder;
            PhInitializeTreeNewColumnMenuEx(&data, PH_TN_COLUMN_MENU_SHOW_RESET_SORT);

            data.Selection = PhShowEMenu(data.Menu, hwnd, PH_EMENU_SHOW_LEFTRIGHT,
                PH_ALIGN_LEFT | PH_ALIGN_TOP, data.MouseEvent->ScreenLocation.x, data.MouseEvent->ScreenLocation.y);
            PhHandleTreeNewColumnMenu(&data);
            PhDeleteTreeNewColumnMenu(&data);
        }
        return TRUE;
    }

    return FALSE;
}

VOID ClearOptionsAdvancedTree(
    _In_ PPH_OPTIONS_ADVANCED_CONTEXT Context
    )
{
    for (ULONG i = 0; i < Context->NodeList->Count; i++)
        DestroyOptionsAdvancedNode(Context->NodeList->Items[i]);

    PhClearHashtable(Context->NodeHashtable);
    PhClearList(Context->NodeList);

    TreeNew_NodesStructured(Context->TreeNewHandle);
}

PPH_OPTIONS_ADVANCED_ROOT_NODE GetSelectedOptionsAdvancedNode(
    _In_ PPH_OPTIONS_ADVANCED_CONTEXT Context
    )
{
    PPH_OPTIONS_ADVANCED_ROOT_NODE optionsNode = NULL;

    for (ULONG i = 0; i < Context->NodeList->Count; i++)
    {
        optionsNode = Context->NodeList->Items[i];

        if (optionsNode->Node.Selected)
            return optionsNode;
    }

    return NULL;
}

_Success_(return)
BOOLEAN GetSelectedOptionsAdvancedNodes(
    _In_ PPH_OPTIONS_ADVANCED_CONTEXT Context,
    _Out_ PPH_OPTIONS_ADVANCED_ROOT_NODE** Nodes,
    _Out_ PULONG NumberOfNodes
    )
{
    PPH_LIST list = PhCreateList(2);

    for (ULONG i = 0; i < Context->NodeList->Count; i++)
    {
        PPH_OPTIONS_ADVANCED_ROOT_NODE node = Context->NodeList->Items[i];

        if (node->Node.Selected)
        {
            PhAddItemList(list, node);
        }
    }

    if (list->Count)
    {
        *Nodes = PhAllocateCopy(list->Items, sizeof(PVOID) * list->Count);
        *NumberOfNodes = list->Count;

        PhDereferenceObject(list);
        return TRUE;
    }

    PhDereferenceObject(list);
    return FALSE;
}

VOID InitializeOptionsAdvancedTree(
    _Inout_ PPH_OPTIONS_ADVANCED_CONTEXT Context
    )
{
    Context->NodeList = PhCreateList(100);
    Context->NodeHashtable = PhCreateHashtable(
        sizeof(PPH_OPTIONS_ADVANCED_ROOT_NODE),
        OptionsAdvancedNodeHashtableEqualFunction,
        OptionsAdvancedNodeHashtableHashFunction,
        100
        );

    PhSetControlTheme(Context->TreeNewHandle, L"explorer");
    TreeNew_SetRedraw(Context->TreeNewHandle, FALSE);
    TreeNew_SetCallback(Context->TreeNewHandle, OptionsAdvancedTreeNewCallback, Context);

    PhAddTreeNewColumnEx(Context->TreeNewHandle, PH_OPTIONS_ADVANCED_COLUMN_ITEM_NAME, TRUE, L"Nombre", 200, PH_ALIGN_LEFT, 0, 0, TRUE);
    PhAddTreeNewColumnEx(Context->TreeNewHandle, PH_OPTIONS_ADVANCED_COLUMN_ITEM_TYPE, TRUE, L"Tipo", 100, PH_ALIGN_LEFT, 1, 0, TRUE);
    PhAddTreeNewColumnEx(Context->TreeNewHandle, PH_OPTIONS_ADVANCED_COLUMN_ITEM_VALUE, TRUE, L"Valor", 200, PH_ALIGN_LEFT, 2, 0, TRUE);
    PhAddTreeNewColumnEx(Context->TreeNewHandle, PH_OPTIONS_ADVANCED_COLUMN_ITEM_DEFAULT, TRUE, L"Predeterminado", 200, PH_ALIGN_LEFT, 3, 0, TRUE);
    PhAddTreeNewColumnEx(Context->TreeNewHandle, PH_OPTIONS_ADVANCED_COLUMN_ITEM_DESCRIPTION, TRUE, L"Description", 300, PH_ALIGN_LEFT, 4, 0, FALSE);

    OptionsAdvancedLoadSchema(Context);

    PhInitializeTreeNewFilterSupport(&Context->TreeFilterSupport, Context->TreeNewHandle, Context->NodeList);

    TreeNew_SetTriState(Context->TreeNewHandle, TRUE);
    TreeNew_SetRedraw(Context->TreeNewHandle, TRUE);

    OptionsAdvancedLoadSettingsTreeList(Context);
}

VOID DeleteOptionsAdvancedTree(
    _In_ PPH_OPTIONS_ADVANCED_CONTEXT Context
    )
{
    PhDeleteTreeNewFilterSupport(&Context->TreeFilterSupport);

    OptionsAdvancedSaveSettingsTreeList(Context);

    for (ULONG i = 0; i < Context->NodeList->Count; i++)
        DestroyOptionsAdvancedNode(Context->NodeList->Items[i]);

    PhDereferenceObject(Context->NodeHashtable);
    PhDereferenceObject(Context->NodeList);

    OptionsAdvancedFreeSchema(Context);
}

#pragma endregion

_Function_class_(PH_SETTINGS_ENUM_CALLBACK)
static BOOLEAN PhpOptionsSettingsCallback(
    _In_ PPH_SETTING Setting,
    _In_ PVOID Context
    )
{
    PPH_OPTIONS_ADVANCED_CONTEXT context = Context;

    AddOptionsAdvancedNode(context, Setting);
    return TRUE;
}

_Function_class_(PH_TN_FILTER_FUNCTION)
BOOLEAN PhpOptionsAdvancedTreeFilterCallback(
    _In_ PPH_TREENEW_NODE Node,
    _In_ PVOID Context
    )
{
    PPH_OPTIONS_ADVANCED_ROOT_NODE node = (PPH_OPTIONS_ADVANCED_ROOT_NODE)Node;
    PPH_OPTIONS_ADVANCED_CONTEXT context = Context;

    if (context->HideModified || context->HideDefault)
    {
        switch (node->Type)
        {
        case StringSettingType:
        case IntegerPairSettingType:
        case ScalableIntegerPairSettingType:
            {
                if (PhEqualString(node->DefaultString, node->ValueString, TRUE))
                {
                    if (context->HideDefault)
                    {
                        return FALSE;
                    }
                }
                else
                {
                    if (context->HideModified)
                    {
                        return FALSE;
                    }
                }
            }
            break;
        case IntegerSettingType:
            {
                ULONG64 integer;

                if (PhStringToInteger64(&node->DefaultString->sr, 16, &integer))
                {
                    if (node->Setting->u.Integer == (ULONG)integer)
                    {
                        if (context->HideDefault)
                        {
                            return FALSE;
                        }
                    }
                    else
                    {
                        if (context->HideModified)
                        {
                            return FALSE;
                        }
                    }
                }
            }
            break;
        }
    }

    if (!context->SearchMatchHandle)
        return TRUE;

    if (PhSearchControlMatch(context->SearchMatchHandle, &node->Name->sr))
        return TRUE;
    if (PhSearchControlMatch(context->SearchMatchHandle, &node->DefaultString->sr))
        return TRUE;
    if (PhSearchControlMatch(context->SearchMatchHandle, &node->ValueString->sr))
        return TRUE;

    return FALSE;
}

_Function_class_(PH_SEARCHCONTROL_CALLBACK)
VOID NTAPI PhpOptionsAdvancedSearchControlCallback(
    _In_ ULONG_PTR MatchHandle,
    _In_opt_ PVOID Context
    )
{
    PPH_OPTIONS_ADVANCED_CONTEXT context = Context;

    assert(context);

    context->SearchMatchHandle = MatchHandle;

    if (!context->SearchMatchHandle)
    {
        // Expand the nodes?
    }

    PhApplyTreeNewFilters(&context->TreeFilterSupport);
}

INT_PTR CALLBACK PhpOptionsAdvancedDlgProc(
    _In_ HWND hwndDlg,
    _In_ UINT uMsg,
    _In_ WPARAM wParam,
    _In_ LPARAM lParam
    )
{
    PPH_OPTIONS_ADVANCED_CONTEXT context;

    if (uMsg == WM_INITDIALOG)
    {
        context = PhAllocateZero(sizeof(PH_OPTIONS_ADVANCED_CONTEXT));
        PhSetWindowContext(hwndDlg, PH_WINDOW_CONTEXT_DEFAULT, context);
    }
    else
    {
        context = PhGetWindowContext(hwndDlg, PH_WINDOW_CONTEXT_DEFAULT);
    }

    if (!context)
        return FALSE;

    switch (uMsg)
    {
    case WM_INITDIALOG:
        {
            context->WindowHandle = hwndDlg;
            context->TreeNewHandle = GetDlgItem(hwndDlg, IDC_SETTINGS);
            context->SearchBoxHandle = GetDlgItem(hwndDlg, IDC_SEARCH);

            PhCreateSearchControl2(
                hwndDlg,
                context->SearchBoxHandle,
                L"Buscar ajustes...",
                SETTING_SEARCH_OPTIONS_REGEX,
                SETTING_SEARCH_OPTIONS_CASE_SENSITIVE,
                PhpOptionsAdvancedSearchControlCallback,
                context
                );

            InitializeOptionsAdvancedTree(context);
            context->TreeFilterEntry = PhAddTreeNewFilter(&context->TreeFilterSupport, PhpOptionsAdvancedTreeFilterCallback, context);

            PhInitializeLayoutManager(&context->LayoutManager, hwndDlg);
            PhAddLayoutItem(&context->LayoutManager, context->SearchBoxHandle, NULL, PH_ANCHOR_TOP | PH_ANCHOR_RIGHT);
            PhAddLayoutItem(&context->LayoutManager, context->TreeNewHandle, NULL, PH_ANCHOR_ALL);

            PhEnumSettings(PhpOptionsSettingsCallback, context);
            TreeNew_NodesStructured(context->TreeNewHandle);
        }
        break;
    case WM_DESTROY:
        {
            PhDeleteLayoutManager(&context->LayoutManager);

            PhRemoveTreeNewFilter(&context->TreeFilterSupport, context->TreeFilterEntry);
             DeleteOptionsAdvancedTree(context);

             PhRemoveWindowContext(hwndDlg, PH_WINDOW_CONTEXT_DEFAULT);
         }
         break;
     case WM_DPICHANGED_AFTERPARENT:
         {
             PhLayoutManagerUpdate(&context->LayoutManager, PhGetWindowDpi(hwndDlg));
             PhLayoutManagerLayout(&context->LayoutManager);
         }
         break;
     case WM_SIZE:
         {
             PhLayoutManagerLayout(&context->LayoutManager);
         }
         break;
     case WM_COMMAND:
         {
             switch (GET_WM_COMMAND_ID(wParam, lParam))
            {
            case IDOK:
            case IDCANCEL:
                {
                    DestroyWindow(hwndDlg);
                }
                break;
            case IDC_FILTEROPTIONS:
                {
                    RECT rect;
                    PPH_EMENU menu;
                    PPH_EMENU_ITEM hidemodifiedMenuItem;
                    PPH_EMENU_ITEM hidedefaultMenuItem;
                    PPH_EMENU_ITEM highlightmodifiedMenuItem;
                    PPH_EMENU_ITEM highlightdefaultMenuItem;
                    PPH_EMENU_ITEM selectedItem;

                    if (!PhGetWindowRect(GetDlgItem(hwndDlg, IDC_FILTEROPTIONS), &rect))
                        break;

                    menu = PhCreateEMenu();
                    PhInsertEMenuItem(menu, hidemodifiedMenuItem = PhCreateEMenuItem(0, PH_OPTIONS_ADVANCED_TREE_ITEM_MENU_HIDE_MODIFIED, L"Ocultar modificados", NULL, NULL), ULONG_MAX);
                    PhInsertEMenuItem(menu, hidedefaultMenuItem = PhCreateEMenuItem(0, PH_OPTIONS_ADVANCED_TREE_ITEM_MENU_HIDE_DEFAULT, L"Ocultar predeterminado", NULL, NULL), ULONG_MAX);
                    PhInsertEMenuItem(menu, PhCreateEMenuSeparator(), ULONG_MAX);
                    PhInsertEMenuItem(menu, highlightmodifiedMenuItem = PhCreateEMenuItem(0, PH_OPTIONS_ADVANCED_TREE_ITEM_MENU_HIGHLIGHT_MODIFIED, L"Resaltar modificados", NULL, NULL), ULONG_MAX);
                    PhInsertEMenuItem(menu, highlightdefaultMenuItem = PhCreateEMenuItem(0, PH_OPTIONS_ADVANCED_TREE_ITEM_MENU_HIGHLIGHT_DEFAULT, L"Resaltar predeterminado", NULL, NULL), ULONG_MAX);

                    if (context->HideModified)
                        hidemodifiedMenuItem->Flags |= PH_EMENU_CHECKED;
                    if (context->HideDefault)
                        hidedefaultMenuItem->Flags |= PH_EMENU_CHECKED;
                    if (context->HighlightModified)
                        highlightmodifiedMenuItem->Flags |= PH_EMENU_CHECKED;
                    if (context->HighlightDefault)
                        highlightdefaultMenuItem->Flags |= PH_EMENU_CHECKED;

                    selectedItem = PhShowEMenu(
                        menu,
                        hwndDlg,
                        PH_EMENU_SHOW_LEFTRIGHT,
                        PH_ALIGN_LEFT | PH_ALIGN_TOP,
                        rect.left,
                        rect.bottom
                        );

                    if (selectedItem && selectedItem->Id)
                    {
                        OptionsAdvancedSetOptionsTreeList(context, selectedItem->Id);

                        PhApplyTreeNewFilters(&context->TreeFilterSupport);
                    }

                    PhDestroyEMenu(menu);
                }
                break;
            case IDC_REFRESH:
                {
                    ClearOptionsAdvancedTree(context);
                    PhEnumSettings(PhpOptionsSettingsCallback, context);

                    PhApplyTreeNewFilters(&context->TreeFilterSupport);
                }
                break;
            case WM_PH_OPTIONS_ADVANCED:
                {
                    PPH_OPTIONS_ADVANCED_ROOT_NODE node;

                    if (node = GetSelectedOptionsAdvancedNode(context))
                    {
                        PH_OPTIONS_ADVANCED_EDIT_CONTEXT editContext;

                        editContext.Setting = node->Setting;
                        editContext.Description = node->Description;
                        editContext.SettingSchema = node->SettingSchema;

                        PhDialogBox(
                            PhInstanceHandle,
                            MAKEINTRESOURCE(IDD_OPTADVEDIT),
                            hwndDlg,
                            PhpOptionsAdvancedEditDlgProc,
                            &editContext
                            );

                        PhMoveReference(
                            &node->ValueString,
                            PhSettingToString(node->Setting->Type, node->Setting)
                            );

                        TreeNew_NodesStructured(context->TreeNewHandle);
                    }
                }
                break;
            case IDC_COPY:
                {
                    PPH_STRING text;

                    text = PhGetTreeNewText(context->TreeNewHandle, 0);
                    PhSetClipboardString(context->TreeNewHandle, &text->sr);
                    PhDereferenceObject(text);
                }
                break;
            case IDC_RESET:
                {
                    PPH_OPTIONS_ADVANCED_ROOT_NODE* nodes;
                    ULONG numberOfNodes;
                    if (!GetSelectedOptionsAdvancedNodes(context, &nodes, &numberOfNodes))
                        break;
                    for (ULONG i = 0; i < numberOfNodes; i++)
                    {
                        PhSettingFromString(
                            nodes[i]->Setting->Type,
                            &nodes[i]->Setting->DefaultValue,
                            NULL,
                            nodes[i]->Setting
                            );
                        PhMoveReference(
                            &nodes[i]->ValueString,
                            PhSettingToString(nodes[i]->Setting->Type, nodes[i]->Setting)
                            );
                    }
                    TreeNew_NodesStructured(context->TreeNewHandle);
                    PhApplyTreeNewFilters(&context->TreeFilterSupport);

                    PhReloadGeneralSection();
                }
                break;
            }
        }
        break;
    case WM_CONTEXTMENU:
        {
            if ((HWND)wParam == context->TreeNewHandle)
            {
                PPH_TREENEW_CONTEXT_MENU contextMenuEvent = (PPH_TREENEW_CONTEXT_MENU)lParam;
                PPH_OPTIONS_ADVANCED_ROOT_NODE* nodes;
                ULONG numberOfNodes;

                if (!GetSelectedOptionsAdvancedNodes(context, &nodes, &numberOfNodes))
                    break;

                if (numberOfNodes != 0)
                {
                    PPH_EMENU menu;
                    PPH_EMENU_ITEM item;

                    menu = PhCreateEMenu();
                    PhInsertEMenuItem(menu, PhCreateEMenuItem(0, IDC_RESET, L"&Restablecer", NULL, NULL), ULONG_MAX);
                    PhInsertEMenuItem(menu, PhCreateEMenuSeparator(), ULONG_MAX);
                    PhInsertEMenuItem(menu, PhCreateEMenuItem(0, IDC_COPY, L"&Copy\bCtrl+C", NULL, NULL), ULONG_MAX);
                    PhInsertCopyCellEMenuItem(menu, IDC_COPY, context->TreeNewHandle, contextMenuEvent->Column);

                    item = PhShowEMenu(
                        menu,
                        hwndDlg,
                        PH_EMENU_SHOW_LEFTRIGHT,
                        PH_ALIGN_LEFT | PH_ALIGN_TOP,
                        contextMenuEvent->Location.x,
                        contextMenuEvent->Location.y
                        );

                    if (item)
                    {
                        if (!PhHandleCopyCellEMenuItem(item))
                        {
                            SendMessage(hwndDlg, WM_COMMAND, item->Id, 0);
                        }
                    }

                    PhDestroyEMenu(menu);
                }

                PhFree(nodes);
            }
        }
        break;
    case WM_NOTIFY:
        {
            REFLECT_MESSAGE_DLG(hwndDlg, context->TreeNewHandle, uMsg, wParam, lParam);
        }
        break;
    case WM_CTLCOLORBTN:
        return HANDLE_WM_CTLCOLORBTN(hwndDlg, wParam, lParam, PhWindowThemeControlColor);
    case WM_CTLCOLORDLG:
        return HANDLE_WM_CTLCOLORDLG(hwndDlg, wParam, lParam, PhWindowThemeControlColor);
    case WM_CTLCOLORSTATIC:
        return HANDLE_WM_CTLCOLORSTATIC(hwndDlg, wParam, lParam, PhWindowThemeControlColor);
    }

    return FALSE;
}

typedef struct _COLOR_ITEM
{
    PCWSTR SettingName;
    PCWSTR UseSettingName;
    ULONG GroupId;
    PCWSTR Name;
    PCWSTR Description;

    BOOLEAN CurrentUse;
    COLORREF CurrentColor;
} COLOR_ITEM, *PCOLOR_ITEM;

typedef enum _PH_OPTIONS_HIGHLIGHTING_GROUP
{
    PH_OPTIONS_HIGHLIGHTING_GROUP_PROCESSES_AND_THREADS,
    PH_OPTIONS_HIGHLIGHTING_GROUP_THREADS,
    PH_OPTIONS_HIGHLIGHTING_GROUP_NETWORK,
    PH_OPTIONS_HIGHLIGHTING_GROUP_ENVIRONMENT,
    PH_OPTIONS_HIGHLIGHTING_GROUP_WMI,
    PH_OPTIONS_HIGHLIGHTING_GROUP_TOKEN,
    PH_OPTIONS_HIGHLIGHTING_GROUP_MEMORY,
    PH_OPTIONS_HIGHLIGHTING_GROUP_MODULES,
    PH_OPTIONS_HIGHLIGHTING_GROUP_IMAGES,
    PH_OPTIONS_HIGHLIGHTING_GROUP_HANDLES,
    PH_OPTIONS_HIGHLIGHTING_GROUP_SERVICES
} PH_OPTIONS_HIGHLIGHTING_GROUP;

#define COLOR_ITEM(SettingName, GroupId, Name, Description) { SettingName, L"Use" SettingName, GroupId, Name, Description }
#define COLOR_ITEM_USE(SettingName, UseSettingName, GroupId, Name, Description) { SettingName, UseSettingName, GroupId, Name, Description }

static COLOR_ITEM ColorItems[] =
{
    COLOR_ITEM(SETTING_COLOR_OWN_PROCESSES, PH_OPTIONS_HIGHLIGHTING_GROUP_PROCESSES_AND_THREADS, L"Procesos propios", L"Procesos que se ejecutan con la misma cuenta de usuario que System Informer."),
    COLOR_ITEM(SETTING_COLOR_SYSTEM_PROCESSES, PH_OPTIONS_HIGHLIGHTING_GROUP_PROCESSES_AND_THREADS, L"Procesos del sistema", L"Procesos que se ejecutan con la cuenta de usuario NT AUTHORITY\SYSTEM."),
    COLOR_ITEM(SETTING_COLOR_SERVICE_PROCESSES, PH_OPTIONS_HIGHLIGHTING_GROUP_PROCESSES_AND_THREADS, L"Procesos de servicio", L"Procesos que alojan uno o más servicios."),
    COLOR_ITEM(SETTING_COLOR_BACKGROUND_PROCESSES, PH_OPTIONS_HIGHLIGHTING_GROUP_PROCESSES_AND_THREADS, L"Procesos en segundo plano", L"Procesos con prioridad de programación en segundo plano."),
    COLOR_ITEM(SETTING_COLOR_JOB_PROCESSES, PH_OPTIONS_HIGHLIGHTING_GROUP_PROCESSES_AND_THREADS, L"Procesos de trabajo", L"Procesos asociados a un trabajo."),
#ifdef _WIN64
    COLOR_ITEM(SETTING_COLOR_WOW64_PROCESSES, PH_OPTIONS_HIGHLIGHTING_GROUP_PROCESSES_AND_THREADS, L"Procesos de 32 bits", L"Procesos que se ejecutan bajo WOW64, es decir, de 32 bits."),
#endif
    COLOR_ITEM(SETTING_COLOR_DEBUGGED_PROCESSES, PH_OPTIONS_HIGHLIGHTING_GROUP_PROCESSES_AND_THREADS, L"Procesos depurados", L"Procesos que se están depurando actualmente."),
    COLOR_ITEM(SETTING_COLOR_ELEVATED_PROCESSES, PH_OPTIONS_HIGHLIGHTING_GROUP_PROCESSES_AND_THREADS, L"Procesos elevados", L"Procesos con privilegios completos en un sistema con UAC habilitado."),
    COLOR_ITEM(SETTING_COLOR_UI_ACCESS_PROCESSES, PH_OPTIONS_HIGHLIGHTING_GROUP_PROCESSES_AND_THREADS, L"Procesos UIAccess", L"Procesos con privilegios UIAccess."),
    COLOR_ITEM(SETTING_COLOR_PICO_PROCESSES, PH_OPTIONS_HIGHLIGHTING_GROUP_PROCESSES_AND_THREADS, L"Procesos Pico", L"Procesos que pertenecen al subsistema de Windows para Linux."),
    COLOR_ITEM(SETTING_COLOR_IMMERSIVE_PROCESSES, PH_OPTIONS_HIGHLIGHTING_GROUP_IMAGES, L"Procesos inmersivos y DLL", L"Procesos y DLL que pertenecen a una aplicación de interfaz moderna."),
    COLOR_ITEM(SETTING_COLOR_SUSPENDED, PH_OPTIONS_HIGHLIGHTING_GROUP_PROCESSES_AND_THREADS, L"Procesos y subprocesos suspendidos", L"Procesos y subprocesos que están suspendidos de la ejecución."),
    COLOR_ITEM(SETTING_COLOR_PARTIALLY_SUSPENDED, PH_OPTIONS_HIGHLIGHTING_GROUP_PROCESSES_AND_THREADS, L"Procesos y subprocesos parcialmente suspendidos", L"Procesos y subprocesos que están parcialmente suspendidos de la ejecución."),
    COLOR_ITEM(SETTING_COLOR_DOT_NET, PH_OPTIONS_HIGHLIGHTING_GROUP_IMAGES, L"Procesos y DLL de .NET", L".NET (es decir, administrados) procesos y DLL."),
    COLOR_ITEM(SETTING_COLOR_PACKED, PH_OPTIONS_HIGHLIGHTING_GROUP_IMAGES, L"Procesos empaquetados", L"Los ejecutables a veces están \"empaquetados\" para reducir su tamaño."),
    COLOR_ITEM(SETTING_COLOR_LOW_IMAGE_COHERENCY, PH_OPTIONS_HIGHLIGHTING_GROUP_IMAGES, L"Baja coherencia de imagen de proceso", L"El archivo de imagen que respalda el proceso tiene una coherencia baja en comparación con la imagen asignada."),
    COLOR_ITEM(SETTING_COLOR_GUI_THREADS, PH_OPTIONS_HIGHLIGHTING_GROUP_PROCESSES_AND_THREADS, L"Subprocesos GUI", L"Subprocesos que han realizado al menos una llamada al sistema relacionada con la GUI."),
    COLOR_ITEM(SETTING_COLOR_THREAD_SUSPENDED, PH_OPTIONS_HIGHLIGHTING_GROUP_THREADS, L"Suspendido", L"Resaltar los subprocesos suspendidos en la lista de Subprocesos."),
    COLOR_ITEM(SETTING_COLOR_THREAD_DELAY_EXECUTION, PH_OPTIONS_HIGHLIGHTING_GROUP_THREADS, L"Ejecución con retardo", L"Resaltar esperas de ejecución con retardo en la lista de Subprocesos."),
    COLOR_ITEM(SETTING_COLOR_THREAD_USER_REQUEST, PH_OPTIONS_HIGHLIGHTING_GROUP_THREADS, L"Solicitud del usuario", L"Resaltar las esperas por solicitud del usuario en la lista de Subprocesos."),
    COLOR_ITEM(SETTING_COLOR_THREAD_ALERT_BY_THREAD_ID, PH_OPTIONS_HIGHLIGHTING_GROUP_THREADS, L"Alerta por ID de subproceso", L"Resaltar esperas de alerta por ID de subproceso en la lista de Subprocesos."),
    COLOR_ITEM(SETTING_COLOR_THREAD_QUEUE, PH_OPTIONS_HIGHLIGHTING_GROUP_THREADS, L"Cola", L"Resaltar las esperas en cola en la lista de Subprocesos."),
    COLOR_ITEM(SETTING_COLOR_THREAD_EXECUTIVE, PH_OPTIONS_HIGHLIGHTING_GROUP_THREADS, L"Ejecutivo", L"Resaltar esperas ejecutivas en la lista de Subprocesos."),
    COLOR_ITEM(SETTING_COLOR_THREAD_GUI_THREADS, PH_OPTIONS_HIGHLIGHTING_GROUP_THREADS, L"Subprocesos GUI", L"Resaltar subprocesos GUI en la lista de Subprocesos."),
    COLOR_ITEM(SETTING_COLOR_NETWORK_UNKNOWN_PROCESS, PH_OPTIONS_HIGHLIGHTING_GROUP_NETWORK, L"Proceso desconocido", L"Resaltar elementos de red cuyo proceso propietario es desconocido."),
    COLOR_ITEM(SETTING_COLOR_NETWORK_SUBSYSTEM_PROCESS, PH_OPTIONS_HIGHLIGHTING_GROUP_NETWORK, L"Proceso del subsistema", L"Resaltar elementos de red propiedad de procesos del subsistema."),
    COLOR_ITEM(SETTING_COLOR_ENVIRONMENT_PROCESS, PH_OPTIONS_HIGHLIGHTING_GROUP_ENVIRONMENT, L"Entorno del proceso", L"Resaltar las variables de entorno del proceso en la página Entorno."),
    COLOR_ITEM(SETTING_COLOR_ENVIRONMENT_USER, PH_OPTIONS_HIGHLIGHTING_GROUP_ENVIRONMENT, L"Entorno del usuario", L"Resaltar las variables de entorno del usuario en la página Entorno."),
    COLOR_ITEM(SETTING_COLOR_ENVIRONMENT_SYSTEM, PH_OPTIONS_HIGHLIGHTING_GROUP_ENVIRONMENT, L"Entorno del sistema", L"Resaltar las variables de entorno del sistema en la página Entorno."),
    COLOR_ITEM(SETTING_COLOR_ENVIRONMENT_CMD, PH_OPTIONS_HIGHLIGHTING_GROUP_ENVIRONMENT, L"Variables CMD", L"Resaltar variables de entorno de estilo CMD en la página Entorno."),
    COLOR_ITEM(SETTING_COLOR_WMI_DEFAULT_NAMESPACE, PH_OPTIONS_HIGHLIGHTING_GROUP_WMI, L"Espacio de nombres predeterminado", L"Resaltar los proveedores en el espacio de nombres WMI predeterminado."),
    COLOR_ITEM(SETTING_COLOR_TOKEN_ENABLED_DEFAULT, PH_OPTIONS_HIGHLIGHTING_GROUP_TOKEN, L"Habilitado de forma predeterminada", L"Grupos o privilegios del token que están activados de forma predeterminada."),
    COLOR_ITEM(SETTING_COLOR_TOKEN_ENABLED, PH_OPTIONS_HIGHLIGHTING_GROUP_TOKEN, L"Enabled", L"Grupos o privilegios del token que están activados."),
    COLOR_ITEM(SETTING_COLOR_TOKEN_DISABLED_DEFAULT, PH_OPTIONS_HIGHLIGHTING_GROUP_TOKEN, L"Deshabilitado de forma predeterminada", L"Grupos o privilegios del token que están desactivados pero activados de forma predeterminada."),
    COLOR_ITEM(SETTING_COLOR_TOKEN_DISABLED, PH_OPTIONS_HIGHLIGHTING_GROUP_TOKEN, L"Disabled", L"Grupos o privilegios del token que están desactivados."),
    COLOR_ITEM(SETTING_COLOR_TOKEN_REMOVED, PH_OPTIONS_HIGHLIGHTING_GROUP_TOKEN, L"Privilegio eliminado", L"Privilegios del token que se han eliminado."),
    COLOR_ITEM(SETTING_COLOR_TOKEN_ELEVATED, PH_OPTIONS_HIGHLIGHTING_GROUP_TOKEN, L"Elevado", L"Grupos o privilegios del token que hacen que el token se notifique como elevado."),
    COLOR_ITEM(SETTING_COLOR_TOKEN_DANGEROUS_FLAG, PH_OPTIONS_HIGHLIGHTING_GROUP_TOKEN, L"Marca peligrosa", L"Indicadores peligrosos del token que están activados."),
    COLOR_ITEM(SETTING_COLOR_TOKEN_NORMAL_FLAG, PH_OPTIONS_HIGHLIGHTING_GROUP_TOKEN, L"Indicador normal", L"Indicadores peligrosos del token que están desactivados."),
    COLOR_ITEM(SETTING_COLOR_MEMORY_PRIVATE_PAGES, PH_OPTIONS_HIGHLIGHTING_GROUP_MEMORY, L"Páginas privadas", L"Resaltar páginas privadas en la lista de Memoria."),
    COLOR_ITEM(SETTING_COLOR_MEMORY_SYSTEM_PAGES, PH_OPTIONS_HIGHLIGHTING_GROUP_MEMORY, L"Páginas del sistema", L"Resaltar las páginas de imagen del sistema en la lista de Memoria."),
    COLOR_ITEM(SETTING_COLOR_MEMORY_CFG_PAGES, PH_OPTIONS_HIGHLIGHTING_GROUP_MEMORY, L"Páginas CFG", L"Resaltar páginas de mapa de bits CFG en la lista de Memoria."),
    COLOR_ITEM(SETTING_COLOR_MEMORY_EXECUTE_PAGES, PH_OPTIONS_HIGHLIGHTING_GROUP_MEMORY, L"Páginas ejecutables", L"Resaltar páginas ejecutables en la lista de Memoria."),
    COLOR_ITEM(SETTING_COLOR_MODULE_UNKNOWN, PH_OPTIONS_HIGHLIGHTING_GROUP_MODULES, L"Módulos no confiables", L"Módulos que no están firmados digitalmente o que no son de confianza por otro motivo."),
    COLOR_ITEM(SETTING_COLOR_MODULE_LOW_IMAGE_COHERENCY, PH_OPTIONS_HIGHLIGHTING_GROUP_MODULES, L"Baja coherencia de imagen", L"Módulos cuyo archivo de imagen de respaldo tiene baja coherencia."),
    COLOR_ITEM(SETTING_COLOR_MODULE_DOT_NET, PH_OPTIONS_HIGHLIGHTING_GROUP_MODULES, L"Módulos .NET", L"Módulos que contienen código .NET administrado."),
    COLOR_ITEM(SETTING_COLOR_MODULE_IMMERSIVE, PH_OPTIONS_HIGHLIGHTING_GROUP_MODULES, L"Módulos inmersivos", L"Módulos asociados a aplicaciones inmersivas o de contenedor de aplicaciones."),
    COLOR_ITEM(SETTING_COLOR_MODULE_RELOCATED, PH_OPTIONS_HIGHLIGHTING_GROUP_MODULES, L"Módulos reubicados", L"Módulos que no se cargaron en su base de imagen preferida."),
    COLOR_ITEM(SETTING_COLOR_MODULE_IMAGEKNOWNDLL, PH_OPTIONS_HIGHLIGHTING_GROUP_MODULES, L"DLL conocidas", L"Módulos cargados desde el conjunto de imágenes KnownDLLs."),
    COLOR_ITEM(SETTING_COLOR_MODULE_SYSTEM, PH_OPTIONS_HIGHLIGHTING_GROUP_MODULES, L"Módulos nativos", L"Módulos nativos estándar cargados, incluidos los módulos de confianza de Microsoft."),
    COLOR_ITEM(SETTING_COLOR_MODULE_MAPPED, PH_OPTIONS_HIGHLIGHTING_GROUP_MODULES, L"Módulos asignados", L"Archivos asignados, imágenes asignadas y módulos de enclave."),
    COLOR_ITEM(SETTING_COLOR_RELOCATED_MODULES, PH_OPTIONS_HIGHLIGHTING_GROUP_IMAGES, L"DLL reubicadas", L"DLL que no se cargaron en sus bases de imagen preferidas."),
    COLOR_ITEM(SETTING_COLOR_PROTECTED_HANDLES, PH_OPTIONS_HIGHLIGHTING_GROUP_HANDLES, L"Identificadores protegidos", L"Identificadores protegidos contra el cierre."),
    COLOR_ITEM(SETTING_COLOR_PROTECTED_PROCESS, PH_OPTIONS_HIGHLIGHTING_GROUP_PROCESSES_AND_THREADS, L"Procesos protegidos", L"Procesos con niveles de protección integrados."),
    COLOR_ITEM(SETTING_COLOR_INHERIT_HANDLES, PH_OPTIONS_HIGHLIGHTING_GROUP_HANDLES, L"Identificadores heredables", L"Identificadores que pueden heredar los procesos secundarios."),
    COLOR_ITEM(SETTING_COLOR_HANDLE_FILTERED, PH_OPTIONS_HIGHLIGHTING_GROUP_PROCESSES_AND_THREADS, L"Procesos filtrados", L"Procesos protegidos por devoluciones de llamada de objetos de identificador."),
    COLOR_ITEM(SETTING_COLOR_UNKNOWN, PH_OPTIONS_HIGHLIGHTING_GROUP_SERVICES, L"DLL y servicios no confiables", L"Servicios y DLL que no están firmados digitalmente."),
    COLOR_ITEM(SETTING_COLOR_SERVICE_DISABLED, PH_OPTIONS_HIGHLIGHTING_GROUP_SERVICES, L"Servicios deshabilitados", L"Servicios que han sido deshabilitados."),
    //COLOR_ITEM(SETTING_COLOR_SERVICE_STOP, L"Servicios detenidos", L"Servicios que no están en ejecución.")
    COLOR_ITEM(SETTING_COLOR_EFFICIENCY_MODE, PH_OPTIONS_HIGHLIGHTING_GROUP_PROCESSES_AND_THREADS, L"Eficiencia energética", L"Procesos y subprocesos con eficiencia energética."),
};

COLORREF NTAPI PhpColorItemColorFunction(
    _In_ INT Index,
    _In_ PVOID Param,
    _In_opt_ PVOID Context
    )
{
    PCOLOR_ITEM item = Param;

    return item->CurrentColor;
}

UINT_PTR CALLBACK PhpColorDlgHookProc(
    _In_ HWND hwndDlg,
    _In_ UINT uMsg,
    _In_ WPARAM wParam,
    _In_ LPARAM lParam
    )
{
    switch (uMsg)
    {
    case WM_INITDIALOG:
        {
            PhCenterWindow(hwndDlg, PhOptionsWindowHandle);

            PhInitializeWindowTheme(hwndDlg, PhEnableThemeSupport);
        }
        break;
    case WM_CTLCOLORBTN:
        return HANDLE_WM_CTLCOLORBTN(hwndDlg, wParam, lParam, PhWindowThemeControlColor);
    case WM_CTLCOLORDLG:
        return HANDLE_WM_CTLCOLORDLG(hwndDlg, wParam, lParam, PhWindowThemeControlColor);
    case WM_CTLCOLORSTATIC:
        return HANDLE_WM_CTLCOLORSTATIC(hwndDlg, wParam, lParam, PhWindowThemeControlColor);
    }

    return FALSE;
}

INT_PTR CALLBACK PhpOptionsHighlightingDlgProc(
    _In_ HWND hwndDlg,
    _In_ UINT uMsg,
    _In_ WPARAM wParam,
    _In_ LPARAM lParam
    )
{
    static PH_LAYOUT_MANAGER LayoutManager;

    switch (uMsg)
    {
    case WM_INITDIALOG:
        {
            // Highlighting Duration
            PhSetDialogItemValue(hwndDlg, IDC_HIGHLIGHTINGDURATION, PhCsHighlightingDuration, FALSE);

            // New Objects
            ColorBox_SetColor(GetDlgItem(hwndDlg, IDC_NEWOBJECTS), PhCsColorNew);
            ColorBox_ThemeSupport(GetDlgItem(hwndDlg, IDC_NEWOBJECTS), PhEnableThemeSupport);

            // Removed Objects
            ColorBox_SetColor(GetDlgItem(hwndDlg, IDC_REMOVEDOBJECTS), PhCsColorRemoved);
            ColorBox_ThemeSupport(GetDlgItem(hwndDlg, IDC_REMOVEDOBJECTS), PhEnableThemeSupport);

            // Enable Window Border Color
            Button_SetCheck(GetDlgItem(hwndDlg, IDC_ENABLE_WINDOW_BORDER_COLOR), PhGetIntegerSetting(SETTING_ENABLE_WINDOW_BORDER_COLOR) ? BST_CHECKED : BST_UNCHECKED);

            // Highlighting
            HighlightingListViewHandle = GetDlgItem(hwndDlg, IDC_LIST);
            PhSetListViewStyle(HighlightingListViewHandle, FALSE, TRUE);
            ListView_SetExtendedListViewStyleEx(HighlightingListViewHandle, LVS_EX_CHECKBOXES, LVS_EX_CHECKBOXES);
            PhAddListViewColumn(HighlightingListViewHandle, 0, 0, 0, LVCFMT_LEFT, 240, L"Nombre");
            PhSetExtendedListView(HighlightingListViewHandle);
            ExtendedListView_SetItemColorFunction(HighlightingListViewHandle, PhpColorItemColorFunction);
            ListView_EnableGroupView(HighlightingListViewHandle, TRUE);
            PhAddListViewGroup(HighlightingListViewHandle, PH_OPTIONS_HIGHLIGHTING_GROUP_PROCESSES_AND_THREADS, L"Procesos");
            PhAddListViewGroup(HighlightingListViewHandle, PH_OPTIONS_HIGHLIGHTING_GROUP_THREADS, L"Subprocesos");
            PhAddListViewGroup(HighlightingListViewHandle, PH_OPTIONS_HIGHLIGHTING_GROUP_NETWORK, L"Network");
            PhAddListViewGroup(HighlightingListViewHandle, PH_OPTIONS_HIGHLIGHTING_GROUP_ENVIRONMENT, L"Entorno");
            PhAddListViewGroup(HighlightingListViewHandle, PH_OPTIONS_HIGHLIGHTING_GROUP_WMI, L"WMI");
            PhAddListViewGroup(HighlightingListViewHandle, PH_OPTIONS_HIGHLIGHTING_GROUP_TOKEN, L"Token");
            PhAddListViewGroup(HighlightingListViewHandle, PH_OPTIONS_HIGHLIGHTING_GROUP_MEMORY, L"Memoria");
            PhAddListViewGroup(HighlightingListViewHandle, PH_OPTIONS_HIGHLIGHTING_GROUP_MODULES, L"Módulos");
            PhAddListViewGroup(HighlightingListViewHandle, PH_OPTIONS_HIGHLIGHTING_GROUP_IMAGES, L"Imágenes y DLL");
            PhAddListViewGroup(HighlightingListViewHandle, PH_OPTIONS_HIGHLIGHTING_GROUP_HANDLES, L"Handles");
            PhAddListViewGroup(HighlightingListViewHandle, PH_OPTIONS_HIGHLIGHTING_GROUP_SERVICES, L"Servicios");

            for (ULONG i = 0; i < RTL_NUMBER_OF(ColorItems); i++)
            {
                INT lvItemIndex;

                lvItemIndex = PhAddListViewGroupItem(HighlightingListViewHandle, ColorItems[i].GroupId, MAXINT, ColorItems[i].Name, &ColorItems[i]);
                ColorItems[i].CurrentColor = PhGetIntegerSetting(ColorItems[i].SettingName);
                ColorItems[i].CurrentUse = !!PhGetIntegerSetting(ColorItems[i].UseSettingName);
                ListView_SetCheckState(HighlightingListViewHandle, lvItemIndex, ColorItems[i].CurrentUse);
            }

            PhInitializeLayoutManager(&LayoutManager, hwndDlg);
            PhAddLayoutItem(&LayoutManager, GetDlgItem(hwndDlg, IDC_ENABLE_WINDOW_BORDER_COLOR), NULL, PH_ANCHOR_TOP | PH_ANCHOR_LEFT);
            PhAddLayoutItem(&LayoutManager, HighlightingListViewHandle, NULL, PH_ANCHOR_ALL);
            PhAddLayoutItem(&LayoutManager, GetDlgItem(hwndDlg, IDC_INFO), NULL, PH_ANCHOR_BOTTOM | PH_ANCHOR_LEFT | PH_LAYOUT_FORCE_INVALIDATE);
            PhAddLayoutItem(&LayoutManager, GetDlgItem(hwndDlg, IDC_ENABLEALL), NULL, PH_ANCHOR_BOTTOM | PH_ANCHOR_RIGHT);
            PhAddLayoutItem(&LayoutManager, GetDlgItem(hwndDlg, IDC_DISABLEALL), NULL, PH_ANCHOR_BOTTOM | PH_ANCHOR_RIGHT);
        }
        break;
    case WM_DESTROY:
        {
            PH_SET_INTEGER_CACHED_SETTING(HighlightingDuration, PhGetDialogItemValue(hwndDlg, IDC_HIGHLIGHTINGDURATION));
            PH_SET_INTEGER_CACHED_SETTING(ColorNew, ColorBox_GetColor(GetDlgItem(hwndDlg, IDC_NEWOBJECTS)));
            PH_SET_INTEGER_CACHED_SETTING(ColorRemoved, ColorBox_GetColor(GetDlgItem(hwndDlg, IDC_REMOVEDOBJECTS)));

            PhSetIntegerSetting(SETTING_ENABLE_WINDOW_BORDER_COLOR, Button_GetCheck(GetDlgItem(hwndDlg, IDC_ENABLE_WINDOW_BORDER_COLOR)) == BST_CHECKED);

            for (ULONG i = 0; i < RTL_NUMBER_OF(ColorItems); i++)
            {
                INT index;

                index = PhFindListViewItemByParam(HighlightingListViewHandle, INT_ERROR, &ColorItems[i]);
                if (index != INT_ERROR)
                    ColorItems[i].CurrentUse = !!ListView_GetCheckState(HighlightingListViewHandle, index);
                PhSetIntegerSetting(ColorItems[i].SettingName, ColorItems[i].CurrentColor);
                PhSetIntegerSetting(ColorItems[i].UseSettingName, ColorItems[i].CurrentUse);
            }

             PhDeleteLayoutManager(&LayoutManager);
         }
         break;
     case WM_DPICHANGED_AFTERPARENT:
         {
             PhLayoutManagerUpdate(&LayoutManager, PhGetWindowDpi(hwndDlg));
             PhLayoutManagerLayout(&LayoutManager);
         }
         break;
     case WM_SIZE:
         {
             PhLayoutManagerLayout(&LayoutManager);

             ExtendedListView_SetColumnWidth(HighlightingListViewHandle, 0, ELVSCW_AUTOSIZE_REMAININGSPACE);
         }
         break;
     case WM_COMMAND:
         {
             switch (GET_WM_COMMAND_ID(wParam, lParam))
             {
             case IDC_ENABLEALL:
                {
                    for (ULONG i = 0; i < RTL_NUMBER_OF(ColorItems); i++)
                    {
                        INT index;

                        index = PhFindListViewItemByParam(HighlightingListViewHandle, INT_ERROR, &ColorItems[i]);
                        if (index != INT_ERROR)
                            ListView_SetCheckState(HighlightingListViewHandle, index, TRUE);
                    }
                }
                break;
            case IDC_DISABLEALL:
                {
                    for (ULONG i = 0; i < RTL_NUMBER_OF(ColorItems); i++)
                    {
                        INT index;

                        index = PhFindListViewItemByParam(HighlightingListViewHandle, INT_ERROR, &ColorItems[i]);
                        if (index != INT_ERROR)
                            ListView_SetCheckState(HighlightingListViewHandle, index, FALSE);
                    }
                }
                break;
            }
        }
        break;
    case WM_NOTIFY:
        {
            LPNMHDR header = (LPNMHDR)lParam;

            switch (header->code)
            {
            case NM_DBLCLK:
                {
                    if (header->hwndFrom == HighlightingListViewHandle)
                    {
                        PCOLOR_ITEM item;

                        if (item = PhGetSelectedListViewItemParam(HighlightingListViewHandle))
                        {
                            CHOOSECOLOR chooseColor;
                            COLORREF customColors[16] = { 0 };

                            PhLoadCustomColorList(SETTING_OPTIONS_CUSTOM_COLOR_LIST, customColors, RTL_NUMBER_OF(customColors));

                            memset(&chooseColor, 0, sizeof(CHOOSECOLOR));
                            chooseColor.lStructSize = sizeof(CHOOSECOLOR);
                            chooseColor.hwndOwner = hwndDlg;
                            chooseColor.rgbResult = item->CurrentColor;
                            chooseColor.lpCustColors = customColors;
                            chooseColor.lpfnHook = PhpColorDlgHookProc;
                            chooseColor.Flags = CC_ANYCOLOR | CC_FULLOPEN | CC_SOLIDCOLOR | CC_ENABLEHOOK | CC_RGBINIT;

                            if (ChooseColor(&chooseColor))
                            {
                                item->CurrentColor = chooseColor.rgbResult;
                                InvalidateRect(HighlightingListViewHandle, NULL, TRUE);
                            }

                            PhSaveCustomColorList(SETTING_OPTIONS_CUSTOM_COLOR_LIST, customColors, RTL_NUMBER_OF(customColors));

                            ListView_SetItemState(HighlightingListViewHandle, -1, 0, LVIS_SELECTED);
                        }
                    }
                }
                break;
            case LVN_GETINFOTIP:
                {
                    if (header->hwndFrom == HighlightingListViewHandle)
                    {
                        NMLVGETINFOTIP *getInfoTip = (NMLVGETINFOTIP *)lParam;
                        PH_STRINGREF tip;
                        PCOLOR_ITEM colorItem;

                        if (PhGetListViewItemParam(HighlightingListViewHandle, getInfoTip->iItem, &colorItem))
                        {
                            PhInitializeStringRefLongHint(&tip, colorItem->Description);
                            PhCopyListViewInfoTip(getInfoTip, &tip);
                        }
                    }
                }
                break;
            }

            REFLECT_MESSAGE_DLG(hwndDlg, HighlightingListViewHandle, uMsg, wParam, lParam);
        }
        break;
    case WM_CONTEXTMENU:
        {
            if ((HWND)wParam == HighlightingListViewHandle)
            {
                POINT point;
                PPH_EMENU menu;
                PPH_EMENU item;
                PCOLOR_ITEM ColorItem;

                point.x = GET_X_LPARAM(lParam);
                point.y = GET_Y_LPARAM(lParam);

                if (point.x == -1 && point.y == -1)
                    PhGetListViewContextMenuPoint(HighlightingListViewHandle, &point);

                if (ColorItem = PhGetSelectedListViewItemParam(HighlightingListViewHandle))
                {
                    menu = PhCreateEMenu();
                    PhInsertEMenuItem(menu, PhCreateEMenuItem(0, IDC_RESET, L"&Restablecer", NULL, NULL), ULONG_MAX);

                    item = PhShowEMenu(
                        menu,
                        hwndDlg,
                        PH_EMENU_SHOW_LEFTRIGHT,
                        PH_ALIGN_LEFT | PH_ALIGN_TOP,
                        point.x,
                        point.y
                        );

                    if (item && item->Id == IDC_RESET)
                    {
                        PPH_SETTING Color;
                        PPH_SETTING UseColor;

                        PH_STRINGREF SettingName;
                        PH_STRINGREF UseSettingName;

                        PhInitializeStringRef(&SettingName, ColorItem->SettingName);
                        PhInitializeStringRef(&UseSettingName, ColorItem->UseSettingName);
                        Color = PhGetSetting(&SettingName);
                        UseColor = PhGetSetting(&UseSettingName);

                        PhSettingFromString(Color->Type, &Color->DefaultValue, NULL, Color);
                        PhSettingFromString(UseColor->Type, &UseColor->DefaultValue, NULL, UseColor);

                        ColorItem->CurrentColor = Color->u.Integer;
                        ColorItem->CurrentUse = !!UseColor->u.Integer;

                        INT index = PhFindListViewItemByParam(HighlightingListViewHandle, INT_ERROR, ColorItem);
                        ListView_SetCheckState(HighlightingListViewHandle, index, ColorItem->CurrentUse);
                        ListView_SetItemState(HighlightingListViewHandle, index, 0, LVIS_SELECTED);
                    }

                    PhDestroyEMenu(menu);
                }
            }
            else if ((HWND)wParam == GetDlgItem(hwndDlg, IDC_NEWOBJECTS) || (HWND)wParam == GetDlgItem(hwndDlg, IDC_REMOVEDOBJECTS))
            {
                POINT point;
                PPH_EMENU menu;
                PPH_EMENU item;

                point.x = GET_X_LPARAM(lParam);
                point.y = GET_Y_LPARAM(lParam);

                menu = PhCreateEMenu();
                PhInsertEMenuItem(menu, PhCreateEMenuItem(0, IDC_RESET, L"&Restablecer", NULL, NULL), ULONG_MAX);

                item = PhShowEMenu(
                    menu,
                    hwndDlg,
                    PH_EMENU_SHOW_LEFTRIGHT,
                    PH_ALIGN_LEFT | PH_ALIGN_TOP,
                    point.x,
                    point.y
                    );

                if (item && item->Id == IDC_RESET)
                {
                    PPH_SETTING Color;
                    BOOLEAN setNew = (HWND)wParam == GetDlgItem(hwndDlg, IDC_NEWOBJECTS);

                    PH_STRINGREF SettingName;

                    PhInitializeStringRef(&SettingName, setNew ? SETTING_COLOR_NEW : SETTING_COLOR_REMOVED);
                    Color = PhGetSetting(&SettingName);

                    PhSettingFromString(Color->Type, &Color->DefaultValue, NULL, Color);

                    ColorBox_SetColor(GetDlgItem(hwndDlg, setNew ? IDC_NEWOBJECTS : IDC_REMOVEDOBJECTS), Color->u.Integer);
                    InvalidateRect(GetDlgItem(hwndDlg, setNew ? IDC_NEWOBJECTS : IDC_REMOVEDOBJECTS), NULL, TRUE);
                }

                PhDestroyEMenu(menu);
            }
        }
        break;
    case WM_CTLCOLORBTN:
        return HANDLE_WM_CTLCOLORBTN(hwndDlg, wParam, lParam, PhWindowThemeControlColor);
    case WM_CTLCOLORDLG:
        return HANDLE_WM_CTLCOLORDLG(hwndDlg, wParam, lParam, PhWindowThemeControlColor);
    case WM_CTLCOLORSTATIC:
        return HANDLE_WM_CTLCOLORSTATIC(hwndDlg, wParam, lParam, PhWindowThemeControlColor);
    }

    return FALSE;
}

typedef struct _PH_TRAYICON_NOTIFY_ITEM
{
    ULONG Bit;
    PWSTR Name;
} PH_TRAYICON_NOTIFY_ITEM, *PPH_TRAYICON_NOTIFY_ITEM;

static PH_TRAYICON_NOTIFY_ITEM TrayIconNotifyItems[] =
{
    { PH_NOTIFY_PROCESS_CREATE, L"Procesos nuevos" },
    { PH_NOTIFY_PROCESS_DELETE, L"Procesos finalizados" },
    { PH_NOTIFY_SERVICE_CREATE, L"Servicios nuevos" },
    { PH_NOTIFY_SERVICE_START, L"Servicios iniciados" },
    { PH_NOTIFY_SERVICE_STOP, L"Servicios detenidos" },
    { PH_NOTIFY_SERVICE_DELETE, L"Servicios eliminados" },
    { PH_NOTIFY_SERVICE_MODIFIED, L"Servicios modificados" },
    { PH_NOTIFY_DEVICE_ARRIVED, L"Dispositivos llegados" },
    { PH_NOTIFY_DEVICE_REMOVED, L"Dispositivos eliminados" },
};

#define PH_OPTIONS_TRAY_ICON_GROUP_NOTIFICATIONS 1
#define PH_OPTIONS_TRAY_ICON_GROUP_TRAY_ICONS 2

INT_PTR CALLBACK PhpOptionsTrayIconDlgProc(
    _In_ HWND hwndDlg,
    _In_ UINT uMsg,
    _In_ WPARAM wParam,
    _In_ LPARAM lParam
    )
{
    static PH_LAYOUT_MANAGER LayoutManager;
    static HWND IconListViewHandle;
    static BOOLEAN RestartRequired;

    switch (uMsg)
    {
    case WM_INITDIALOG:
        {
            ULONG notifyMask;

            RestartRequired = FALSE;

            Button_SetCheck(GetDlgItem(hwndDlg, IDC_TRAYICON_LAZYSTART), PhGetIntegerSetting(SETTING_ICON_TRAY_LAZY_START_DELAY) ? BST_CHECKED : BST_UNCHECKED);
            Button_SetCheck(GetDlgItem(hwndDlg, IDC_TRAYICON_PERSISTLAYOUT), PhGetIntegerSetting(SETTING_ICON_TRAY_PERSIST_GUID_ENABLED) ? BST_CHECKED : BST_UNCHECKED);
            Button_SetCheck(GetDlgItem(hwndDlg, IDC_TRAYICON_TRANSPARENTICONS), PhGetIntegerSetting(SETTING_ICON_TRANSPARENCY_ENABLED) ? BST_CHECKED : BST_UNCHECKED);
            Button_SetCheck(GetDlgItem(hwndDlg, IDC_TRAYICON_SINGLECLICK), PhGetIntegerSetting(SETTING_ICON_SINGLE_CLICK) ? BST_CHECKED : BST_UNCHECKED);

            notifyMask = PhGetIntegerSetting(SETTING_ICON_NOTIFY_MASK);

            IconListViewHandle = GetDlgItem(hwndDlg, IDC_TRAYICON_ICONLIST);
            PhSetListViewStyle(IconListViewHandle, FALSE, TRUE);
            ListView_SetExtendedListViewStyleEx(IconListViewHandle, LVS_EX_CHECKBOXES, LVS_EX_CHECKBOXES);
            PhAddListViewColumn(IconListViewHandle, 0, 0, 0, LVCFMT_LEFT, 230, L"Nombre");
            PhSetExtendedListView(IconListViewHandle);
            ListView_EnableGroupView(IconListViewHandle, TRUE);
            PhAddListViewGroup(IconListViewHandle, PH_OPTIONS_TRAY_ICON_GROUP_NOTIFICATIONS, L"Notificaciones");
            PhAddListViewGroup(IconListViewHandle, PH_OPTIONS_TRAY_ICON_GROUP_TRAY_ICONS, L"Iconos de la bandeja");

            for (ULONG i = 0; i < RTL_NUMBER_OF(TrayIconNotifyItems); i++)
            {
                INT lvItemIndex;

                if ((TrayIconNotifyItems[i].Bit == PH_NOTIFY_DEVICE_ARRIVED || TrayIconNotifyItems[i].Bit == PH_NOTIFY_DEVICE_REMOVED) &&
                    WindowsVersion < WINDOWS_10)
                {
                    continue;
                }

                lvItemIndex = PhAddListViewGroupItem(
                    IconListViewHandle,
                    PH_OPTIONS_TRAY_ICON_GROUP_NOTIFICATIONS,
                    MAXINT,
                    TrayIconNotifyItems[i].Name,
                    &TrayIconNotifyItems[i]
                    );
                ListView_SetCheckState(IconListViewHandle, lvItemIndex, !!(notifyMask & TrayIconNotifyItems[i].Bit));
            }

            for (ULONG i = 0; i < PhTrayIconItemList->Count; i++)
            {
                PPH_NF_ICON icon = PhTrayIconItemList->Items[i];
                INT lvItemIndex;

                lvItemIndex = PhAddListViewGroupItem(
                    IconListViewHandle,
                    PH_OPTIONS_TRAY_ICON_GROUP_TRAY_ICONS,
                    MAXINT,
                    icon->Text,
                    icon
                    );
                ListView_SetCheckState(IconListViewHandle, lvItemIndex, !!(icon->Flags & PH_NF_ICON_ENABLED));
            }

            PhInitializeLayoutManager(&LayoutManager, hwndDlg);
            PhAddLayoutItem(&LayoutManager, GetDlgItem(hwndDlg, IDC_TRAYICON_LAZYSTART), NULL, PH_ANCHOR_TOP | PH_ANCHOR_LEFT);
            PhAddLayoutItem(&LayoutManager, GetDlgItem(hwndDlg, IDC_TRAYICON_PERSISTLAYOUT), NULL, PH_ANCHOR_TOP | PH_ANCHOR_LEFT);
            PhAddLayoutItem(&LayoutManager, GetDlgItem(hwndDlg, IDC_TRAYICON_TRANSPARENTICONS), NULL, PH_ANCHOR_TOP | PH_ANCHOR_LEFT);
            PhAddLayoutItem(&LayoutManager, GetDlgItem(hwndDlg, IDC_TRAYICON_SINGLECLICK), NULL, PH_ANCHOR_TOP | PH_ANCHOR_LEFT);
            PhAddLayoutItem(&LayoutManager, GetDlgItem(hwndDlg, IDC_TRAYICON_RESETLAYOUT), NULL, PH_ANCHOR_TOP | PH_ANCHOR_LEFT);
            PhAddLayoutItem(&LayoutManager, IconListViewHandle, NULL, PH_ANCHOR_ALL);
        }
        break;
    case WM_DESTROY:
        {
            EXTERN_C BOOLEAN PhNfTransparencyEnabled;
            BOOLEAN lazyStart;
            BOOLEAN persistLayout;
            BOOLEAN transparentIcons;
            ULONG notifyMask;

            lazyStart = Button_GetCheck(GetDlgItem(hwndDlg, IDC_TRAYICON_LAZYSTART)) == BST_CHECKED;
            persistLayout = Button_GetCheck(GetDlgItem(hwndDlg, IDC_TRAYICON_PERSISTLAYOUT)) == BST_CHECKED;
            transparentIcons = Button_GetCheck(GetDlgItem(hwndDlg, IDC_TRAYICON_TRANSPARENTICONS)) == BST_CHECKED;

            if (lazyStart != !!PhGetIntegerSetting(SETTING_ICON_TRAY_LAZY_START_DELAY))
                RestartRequired = TRUE;
            if (persistLayout != !!PhGetIntegerSetting(SETTING_ICON_TRAY_PERSIST_GUID_ENABLED))
                RestartRequired = TRUE;
            if (transparentIcons != !!PhGetIntegerSetting(SETTING_ICON_TRANSPARENCY_ENABLED))
                RestartRequired = TRUE;

            PhSetIntegerSetting(SETTING_ICON_TRAY_LAZY_START_DELAY, lazyStart);
            PhSetIntegerSetting(SETTING_ICON_TRAY_PERSIST_GUID_ENABLED, persistLayout);
            PhSetIntegerSetting(SETTING_ICON_TRANSPARENCY_ENABLED, transparentIcons);
            PhSetIntegerSetting(SETTING_ICON_SINGLE_CLICK, Button_GetCheck(GetDlgItem(hwndDlg, IDC_TRAYICON_SINGLECLICK)) == BST_CHECKED);

            PhNfTransparencyEnabled = transparentIcons;

            notifyMask = 0;

            for (ULONG i = 0; i < RTL_NUMBER_OF(TrayIconNotifyItems); i++)
            {
                INT index;

                index = PhFindListViewItemByParam(IconListViewHandle, INT_ERROR, &TrayIconNotifyItems[i]);
                if (index != INT_ERROR && ListView_GetCheckState(IconListViewHandle, index))
                    notifyMask |= TrayIconNotifyItems[i].Bit;
            }

            PhSetIntegerSetting(SETTING_ICON_NOTIFY_MASK, notifyMask);
            PhMwpNotifyIconNotifyMask = notifyMask;

            for (ULONG i = 0; i < PhTrayIconItemList->Count; i++)
            {
                PPH_NF_ICON icon = PhTrayIconItemList->Items[i];
                INT index;

                index = PhFindListViewItemByParam(IconListViewHandle, INT_ERROR, icon);
                if (index != INT_ERROR)
                    PhNfSetVisibleIcon(icon, !!ListView_GetCheckState(IconListViewHandle, index));
            }

            PhNfSaveSettings();

            if (RestartRequired)
                PhShowOptionsRestartRequired(hwndDlg);

            PhDeleteLayoutManager(&LayoutManager);
        }
        break;
    case WM_DPICHANGED_AFTERPARENT:
        {
            PhLayoutManagerUpdate(&LayoutManager, PhGetWindowDpi(hwndDlg));
            PhLayoutManagerLayout(&LayoutManager);
        }
        break;
    case WM_SIZE:
        {
            PhLayoutManagerLayout(&LayoutManager);

            ExtendedListView_SetColumnWidth(IconListViewHandle, 0, ELVSCW_AUTOSIZE_REMAININGSPACE);
        }
        break;
    case WM_COMMAND:
        {
            switch (GET_WM_COMMAND_ID(wParam, lParam))
            {
            case IDC_TRAYICON_RESETLAYOUT:
                {
                    EXTERN_C VOID PhNfLoadGuids(VOID);

                    PhSetStringSetting(SETTING_ICON_TRAY_GUIDS, L"");
                    PhNfLoadGuids();
                    RestartRequired = TRUE;
                }
                break;
            }
        }
        break;
    case WM_CTLCOLORBTN:
        return HANDLE_WM_CTLCOLORBTN(hwndDlg, wParam, lParam, PhWindowThemeControlColor);
    case WM_CTLCOLORDLG:
        return HANDLE_WM_CTLCOLORDLG(hwndDlg, wParam, lParam, PhWindowThemeControlColor);
    case WM_CTLCOLORSTATIC:
        return HANDLE_WM_CTLCOLORSTATIC(hwndDlg, wParam, lParam, PhWindowThemeControlColor);
    }

    return FALSE;
}

INT_PTR CALLBACK PhpOptionsThemesDlgProc(
    _In_ HWND hwndDlg,
    _In_ UINT uMsg,
    _In_ WPARAM wParam,
    _In_ LPARAM lParam
    )
{
    static PH_LAYOUT_MANAGER LayoutManager;

    switch (uMsg)
    {
    case WM_INITDIALOG:
        {
            HWND comboHandle;

            PhInitializeLayoutManager(&LayoutManager, hwndDlg);
            PhAddLayoutItem(&LayoutManager, GetDlgItem(hwndDlg, IDC_ENABLETHEME), NULL, PH_ANCHOR_TOP | PH_ANCHOR_LEFT);
            PhAddLayoutItem(&LayoutManager, GetDlgItem(hwndDlg, IDC_THEMEMODE), NULL, PH_ANCHOR_TOP | PH_ANCHOR_LEFT);

            SetDlgItemCheckForSetting(hwndDlg, IDC_ENABLETHEME, SETTING_ENABLE_THEME_SUPPORT);

            comboHandle = GetDlgItem(hwndDlg, IDC_THEMEMODE);
            ComboBox_AddString(comboHandle, L"Automático");
            ComboBox_AddString(comboHandle, L"Light");
            ComboBox_AddString(comboHandle, L"Oscuro");
            ComboBox_AddString(comboHandle, L"Personalizado");
            ComboBox_SetCurSel(comboHandle, PhGetIntegerSetting(SETTING_THEME_MODE));
            EnableWindow(comboHandle, PhGetIntegerSetting(SETTING_ENABLE_THEME_SUPPORT) != 0);
        }
        break;
    case WM_DESTROY:
        {
            PhDeleteLayoutManager(&LayoutManager);
        }
        break;
    case WM_COMMAND:
        {
            if (GET_WM_COMMAND_ID(wParam, lParam) == IDC_ENABLETHEME && !ThemeListViewStateInitializing)
            {
                PhpApplyThemeSupportSetting(hwndDlg);
            }
            else if (GET_WM_COMMAND_ID(wParam, lParam) == IDC_THEMEMODE &&
                GET_WM_COMMAND_CMD(wParam, lParam) == CBN_SELCHANGE && !ThemeListViewStateInitializing)
            {
                PhpApplyThemeModeSetting(hwndDlg);
            }
        }
        break;
    }

    return FALSE;
}

static COLOR_ITEM PhpOptionsGraphColorItems[] =
{
    COLOR_ITEM(SETTING_COLOR_CPU_KERNEL, 0, L"CPU del núcleo", L"CPU del núcleo"),
    COLOR_ITEM(SETTING_COLOR_CPU_USER, 0, L"CPU del usuario", L"CPU del usuario"),
    COLOR_ITEM(SETTING_COLOR_IO_READ_OTHER, 0, L"E/S R+O", L"E/S R+O"),
    COLOR_ITEM(SETTING_COLOR_IO_WRITE, 0, L"E/S W", L"E/S W"),
    COLOR_ITEM(SETTING_COLOR_PRIVATE, 0, L"Bytes privados", L"Bytes privados"),
    COLOR_ITEM(SETTING_COLOR_PHYSICAL, 0, L"Memoria física", L"Memoria física"),
    COLOR_ITEM(SETTING_COLOR_POWER_USAGE, 0, L"Uso de energía", L"Uso de energía"),
    COLOR_ITEM(SETTING_COLOR_TEMPERATURE, 0, L"Temperatura", L"Temperatura"),
    COLOR_ITEM(SETTING_COLOR_FAN_RPM, 0, L"RPM del ventilador", L"RPM del ventilador"),
};
static HWND PhpGraphListViewHandle = NULL;

INT_PTR CALLBACK PhpOptionsGraphsDlgProc(
    _In_ HWND hwndDlg,
    _In_ UINT uMsg,
    _In_ WPARAM wParam,
    _In_ LPARAM lParam
    )
{
    static PH_LAYOUT_MANAGER LayoutManager;

    switch (uMsg)
    {
    case WM_INITDIALOG:
        {
            // Show Text
            SetDlgItemCheckForSetting(hwndDlg, IDC_SHOWTEXT, SETTING_GRAPH_SHOW_TEXT);
            SetDlgItemCheckForSetting(hwndDlg, IDC_USEOLDCOLORS, SETTING_GRAPH_COLOR_MODE);
            SetDlgItemCheckForSetting(hwndDlg, IDC_SHOWCOMMITINSUMMARY, SETTING_SHOW_COMMIT_IN_SUMMARY);

            // Highlighting
            PhpGraphListViewHandle = GetDlgItem(hwndDlg, IDC_LIST);
            PhSetListViewStyle(PhpGraphListViewHandle, FALSE, TRUE);
            PhAddListViewColumn(PhpGraphListViewHandle, 0, 0, 0, LVCFMT_LEFT, 240, L"Nombre");
            PhSetExtendedListView(PhpGraphListViewHandle);
            ExtendedListView_SetItemColorFunction(PhpGraphListViewHandle, PhpColorItemColorFunction);

            for (ULONG i = 0; i < RTL_NUMBER_OF(PhpOptionsGraphColorItems); i++)
            {
                INT lvItemIndex = PhAddListViewItem(PhpGraphListViewHandle, MAXINT, PhpOptionsGraphColorItems[i].Name, &PhpOptionsGraphColorItems[i]);
                PhpOptionsGraphColorItems[i].CurrentColor = PhGetIntegerSetting(PhpOptionsGraphColorItems[i].SettingName);
            }

            PhInitializeLayoutManager(&LayoutManager, hwndDlg);
            PhAddLayoutItem(&LayoutManager, PhpGraphListViewHandle, NULL, PH_ANCHOR_ALL);

            if (PhGetIntegerSetting(SETTING_GRAPH_COLOR_MODE))
                EnableWindow(PhpGraphListViewHandle, TRUE);
            else if (PhEnableThemeSupport)
            {
                ShowWindow(PhpGraphListViewHandle, SW_HIDE);
                EnableWindow(PhpGraphListViewHandle, TRUE);
            }
        }
        break;
    case WM_DESTROY:
        {
            SetSettingForDlgItemCheck(hwndDlg, IDC_SHOWTEXT, SETTING_GRAPH_SHOW_TEXT);
            SetSettingForDlgItemCheck(hwndDlg, IDC_USEOLDCOLORS, SETTING_GRAPH_COLOR_MODE);
            SetSettingForDlgItemCheck(hwndDlg, IDC_SHOWCOMMITINSUMMARY, SETTING_SHOW_COMMIT_IN_SUMMARY);

            for (ULONG i = 0; i < RTL_NUMBER_OF(PhpOptionsGraphColorItems); i++)
            {
                PhSetIntegerSetting(PhpOptionsGraphColorItems[i].SettingName, PhpOptionsGraphColorItems[i].CurrentColor);
            }

             PhDeleteLayoutManager(&LayoutManager);
         }
         break;
     case WM_DPICHANGED_AFTERPARENT:
         {
             PhLayoutManagerUpdate(&LayoutManager, PhGetWindowDpi(hwndDlg));
             PhLayoutManagerLayout(&LayoutManager);
         }
         break;
     case WM_SIZE:
         {
             PhLayoutManagerLayout(&LayoutManager);

             ExtendedListView_SetColumnWidth(PhpGraphListViewHandle, 0, ELVSCW_AUTOSIZE_REMAININGSPACE);
         }
         break;
     case WM_COMMAND:
         {
             switch (GET_WM_COMMAND_ID(wParam, lParam))
             {
             case IDC_USEOLDCOLORS:
                {
                    ListView_SetItemState(PhpGraphListViewHandle, -1, 0, LVIS_SELECTED); // deselect all items

                    if (PhEnableThemeSupport)
                        ShowWindow(PhpGraphListViewHandle, Button_GetCheck(GET_WM_COMMAND_HWND(wParam, lParam)) == BST_CHECKED ? SW_SHOW : SW_HIDE);
                    else
                        EnableWindow(PhpGraphListViewHandle, Button_GetCheck(GET_WM_COMMAND_HWND(wParam, lParam)) == BST_CHECKED);
                }
                break;
            }
        }
        break;
    case WM_NOTIFY:
        {
            LPNMHDR header = (LPNMHDR)lParam;

            switch (header->code)
            {
            case NM_DBLCLK:
                {
                    if (header->hwndFrom == PhpGraphListViewHandle)
                    {
                        PCOLOR_ITEM item;

                        if (item = PhGetSelectedListViewItemParam(PhpGraphListViewHandle))
                        {
                            CHOOSECOLOR chooseColor;
                            COLORREF customColors[16] = { 0 };

                            PhLoadCustomColorList(SETTING_OPTIONS_CUSTOM_COLOR_LIST, customColors, RTL_NUMBER_OF(customColors));

                            memset(&chooseColor, 0, sizeof(CHOOSECOLOR));
                            chooseColor.lStructSize = sizeof(CHOOSECOLOR);
                            chooseColor.hwndOwner = hwndDlg;
                            chooseColor.rgbResult = item->CurrentColor;
                            chooseColor.lpCustColors = customColors;
                            chooseColor.lpfnHook = PhpColorDlgHookProc;
                            chooseColor.Flags = CC_ANYCOLOR | CC_FULLOPEN | CC_SOLIDCOLOR | CC_ENABLEHOOK | CC_RGBINIT;

                            if (ChooseColor(&chooseColor))
                            {
                                item->CurrentColor = chooseColor.rgbResult;
                                InvalidateRect(PhpGraphListViewHandle, NULL, TRUE);
                            }

                            PhSaveCustomColorList(SETTING_OPTIONS_CUSTOM_COLOR_LIST, customColors, RTL_NUMBER_OF(customColors));

                            ListView_SetItemState(PhpGraphListViewHandle, -1, 0, LVIS_SELECTED);
                        }
                    }
                }
                break;
            case LVN_GETINFOTIP:
                {
                    if (header->hwndFrom == PhpGraphListViewHandle)
                    {
                        NMLVGETINFOTIP* getInfoTip = (NMLVGETINFOTIP*)lParam;
                        PH_STRINGREF tip;

                        PhInitializeStringRefLongHint(&tip, ColorItems[getInfoTip->iItem].Description);
                        PhCopyListViewInfoTip(getInfoTip, &tip);
                    }
                }
                break;
            }

            if (IsWindowEnabled(PhpGraphListViewHandle)) // HACK: Move to WM_COMMAND (dmex)
            {
                REFLECT_MESSAGE_DLG(hwndDlg, PhpGraphListViewHandle, uMsg, wParam, lParam);
            }
        }
        break;
    case WM_CONTEXTMENU:
        {
            if ((HWND)wParam == PhpGraphListViewHandle)
            {
                POINT point;
                PPH_EMENU menu;
                PPH_EMENU item;
                PCOLOR_ITEM ColorItem;

                point.x = GET_X_LPARAM(lParam);
                point.y = GET_Y_LPARAM(lParam);

                if (point.x == -1 && point.y == -1)
                    PhGetListViewContextMenuPoint(PhpGraphListViewHandle, &point);

                if (ColorItem = PhGetSelectedListViewItemParam(PhpGraphListViewHandle))
                {
                    menu = PhCreateEMenu();
                    PhInsertEMenuItem(menu, PhCreateEMenuItem(0, IDC_RESET, L"&Restablecer", NULL, NULL), ULONG_MAX);

                    item = PhShowEMenu(
                        menu,
                        hwndDlg,
                        PH_EMENU_SHOW_LEFTRIGHT,
                        PH_ALIGN_LEFT | PH_ALIGN_TOP,
                        point.x,
                        point.y
                        );

                    if (item && item->Id == IDC_RESET)
                    {
                        PPH_SETTING Color;

                        PH_STRINGREF SettingName;

                        PhInitializeStringRef(&SettingName, ColorItem->SettingName);
                        Color = PhGetSetting(&SettingName);

                        PhSettingFromString(Color->Type, &Color->DefaultValue, NULL, Color);

                        ColorItem->CurrentColor = Color->u.Integer;

                        ListView_SetItemState(PhpGraphListViewHandle, -1, 0, LVIS_SELECTED);
                    }

                    PhDestroyEMenu(menu);
                }
            }
        }
        break;
    case WM_CTLCOLORBTN:
        return HANDLE_WM_CTLCOLORBTN(hwndDlg, wParam, lParam, PhWindowThemeControlColor);
    case WM_CTLCOLORDLG:
        return HANDLE_WM_CTLCOLORDLG(hwndDlg, wParam, lParam, PhWindowThemeControlColor);
    case WM_CTLCOLORSTATIC:
        return HANDLE_WM_CTLCOLORSTATIC(hwndDlg, wParam, lParam, PhWindowThemeControlColor);
    }

    return FALSE;
}
