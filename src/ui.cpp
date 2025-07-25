// ui.cpp
//
// Copyright (c) 2019-2024 Kristofer Berggren
// All rights reserved.
//
// falaclient is distributed under the MIT license, see LICENSE for details.

#include "ui.h"

#include <algorithm>
#include <climits>
#include <cstdint>
#include <sstream>
#include <iomanip>

#include "addressbook.h"
#include "html_parser.h"
#include "flag.h"
#include "loghelp.h"
#include "maphelp.h"
#include "offlinequeue.h"
#include "sethelp.h"
#include "sleepdetect.h"
#include "status.h"
#include "util.h"
#include "version.h"

#ifdef __OpenBSD__
// OpenBSD ncurses doesn't have mvwaddnwstr, so we create a compatibility function
static inline int mvwaddnwstr_compat(WINDOW* win, int y, int x, const wchar_t* wstr, int n)
{
  std::wstring ws(wstr, n);
  std::string s = Util::ToString(ws);
  return mvwaddnstr(win, y, x, s.c_str(), s.size());
}
#define mvwaddnwstr mvwaddnwstr_compat

// OpenBSD ncurses doesn't have get_wch, so we use getch instead
static inline int get_wch_compat(wint_t* wch)
{
  int ch = getch();
  if (ch == ERR)
  {
    return ERR;
  }
  *wch = ch;
  return OK;
}
#define get_wch get_wch_compat
#endif

bool Ui::s_Running = false;

Ui::Ui(const std::string& p_Inbox, const std::string& p_Address, const std::string& p_Name,
       uint32_t p_PrefetchLevel, bool p_PrefetchAllHeaders)
  : m_Inbox(p_Inbox)
  , m_Address(p_Address)
  , m_Name(p_Name)
  , m_PrefetchLevel(p_PrefetchLevel)
  , m_PrefetchAllHeaders(p_PrefetchAllHeaders)
{
  m_CurrentFolder = p_Inbox;
  Init();
  InitWindows();
}

Ui::~Ui()
{
  CleanupWindows();
  Cleanup();
}

void Ui::Init()
{
  const std::map<std::string, std::string> defaultConfig =
  {
    { "compose_line_wrap", "0" },
    { "respect_format_flowed", "1" },
    { "rewrap_quoted_lines", "1" },
    { "help_enabled", "1" },
    { "persist_file_selection_dir", "1" },
    { "persist_find_query", "0" },
    { "persist_folder_filter", "1" },
    { "persist_search_query", "0" },
    { "plain_text", "1" },
    { "show_progress", "1" },
    { "new_msg_bell", "1" },
    { "quit_without_confirm", "1" },
    { "send_without_confirm", "0" },
    { "cancel_without_confirm", "0" },
    { "postpone_without_confirm", "0" },
    { "delete_without_confirm", "0" },
    { "show_embedded_images", "1" },
    { "show_rich_header", "0" },
    { "markdown_html_compose", "0" },
    { "key_prev_msg", "p" },
    { "key_next_msg", "n" },
    { "key_reply_all", "r" },
    { "key_reply_sender", "R" },
    { "key_forward", "f" },
    { "key_forward_attached", "F" },
    { "key_delete", "d" },
    { "key_compose", "c" },
    { "key_compose_copy", "C" },
    { "key_toggle_unread", "u" },
    { "key_move", "M" },
    { "key_auto_move", "m" },
    { "key_refresh", "l" },
    { "key_quit", "q" },
    { "key_toggle_text_html", "t" },
    { "key_cancel", "KEY_CTRLC" },
    { "key_send", "KEY_CTRLX" },
    { "key_delete_char_after_cursor", "KEY_CTRLD" },
    { "key_delete_line_after_cursor", "KEY_CTRLK" },
    { "key_delete_line_before_cursor", "KEY_CTRLU" },
    { "key_open", "." },
    { "key_back", "," },
    { "key_goto_folder", "g" },
    { "key_goto_inbox", "i" },
    { "key_to_select", "KEY_CTRLT" },
    { "key_save_file", "s" },
    { "key_ext_editor", "KEY_CTRLW" },
    { "key_ext_pager", "e" },
    { "key_postpone", "KEY_CTRLO" },
    { "key_othercmd_help", "o" },
    { "key_export", "x" },
    { "key_import", "z" },
    { "key_rich_header", "KEY_CTRLR" },
    { "key_ext_html_viewer", "v" },
    { "key_ext_html_preview", "KEY_CTRLV" },
    { "key_ext_msg_viewer", "w" },
    { "key_search", "/" },
    { "key_search_current_subject", "=" },
    { "key_search_current_name", "-" },
    { "key_find", "/" },
    { "key_find_next", "?" },
    { "key_sync", "s" },
    { "key_toggle_markdown_compose", "KEY_CTRLN" },
#if defined(__APPLE__)
    { "key_backward_word", "\\033\\142" }, // opt-left
    { "key_forward_word", "\\033\\146" }, // opt-right
    { "key_backward_kill_word", "\\033\\177" }, // opt-backspace
    { "key_kill_word", "\\033\\010" }, // opt-delete
#else // defined(__linux__)
    { "key_backward_word", "\\1040" }, // alt-left
    { "key_forward_word", "\\1057" }, // alt-right
    { "key_backward_kill_word", "\\033\\177" }, // alt-backspace
    { "key_kill_word", "\\1006" }, // alt-delete
#endif
    { "key_begin_line", "KEY_CTRLA" },
    { "key_end_line", "KEY_CTRLE" },
    { "key_prev_page", "KEY_PPAGE" },
    { "key_next_page", "KEY_NPAGE" },
    { "key_prev_page_compose", "KEY_PPAGE" },
    { "key_next_page_compose", "KEY_NPAGE" },
    { "key_filter_sort_reset", "`" },
    { "key_filter_show_unread", "1" },
    { "key_filter_show_has_attachments", "2" },
    { "key_filter_show_current_date", "3" },
    { "key_filter_show_current_name", "4" },
    { "key_filter_show_current_subject", "5" },
    { "key_sort_unread", "!" },
    { "key_sort_has_attachments", "@" },
    { "key_sort_date", "#" },
    { "key_sort_name", "$" },
    { "key_sort_subject", "%" },
    { "key_jump_to", "j" },
    { "key_toggle_full_header", "h" },
    { "key_select_item", "KEY_SPACE" },
    { "key_select_all", "a" },
    { "key_search_show_folder", "\\" },
    { "key_spell", "KEY_CTRLS" },
    { "colors_enabled", "1" },
    { "attachment_indicator", "\xF0\x9F\x93\x8E" },
    { "bottom_reply", "0" },
    { "compose_backup_interval", "10" },
    { "persist_sortfilter", "1" },
    { "persist_selection_on_sortfilter_change", "1" },
    { "unread_indicator", "N" },
    { "invalid_input_notify", "1" },
    { "full_header_include_local", "0" },
    { "tab_size", "8" },
    { "search_show_folder", "0" },
    { "localized_subject_prefixes", "" },
    { "signature", "0" },
    { "terminal_title", "" },
    { "top_bar_show_version", "0" },
  };
  const std::string configPath(Util::GetApplicationDir() + std::string("ui.conf"));
  m_Config = Config(configPath, defaultConfig);
  m_Config.LogParams();

  m_TerminalTitle = m_Config.Get("terminal_title");

  if (!m_TerminalTitle.empty())
  {
    printf("\033]0;%s\007", m_TerminalTitle.c_str());
  }

  setlocale(LC_ALL, "");
  initscr();
  noecho();
  cbreak();
  keypad(stdscr, TRUE);
  curs_set(0);
  timeout(0);

  m_ComposeLineWrap = Util::ToInteger(m_Config.Get("compose_line_wrap"));
  m_RespectFormatFlowed = m_Config.Get("respect_format_flowed") == "1";
  m_RewrapQuotedLines = m_Config.Get("rewrap_quoted_lines") == "1";
  m_HelpEnabled = m_Config.Get("help_enabled") == "1";
  m_PersistFileSelectionDir = m_Config.Get("persist_file_selection_dir") == "1";
  m_PersistFindQuery = m_Config.Get("persist_find_query") == "1";
  m_PersistFolderFilter = m_Config.Get("persist_folder_filter") == "1";
  m_PersistSearchQuery = m_Config.Get("persist_search_query") == "1";
  m_Plaintext = m_Config.Get("plain_text") == "1";
  m_MarkdownHtmlCompose = m_Config.Get("markdown_html_compose") == "1";
  m_KeyPrevMsg = Util::GetKeyCode(m_Config.Get("key_prev_msg"));
  m_KeyNextMsg = Util::GetKeyCode(m_Config.Get("key_next_msg"));
  m_KeyReplyAll = Util::GetKeyCode(m_Config.Get("key_reply_all"));
  m_KeyReplySender = Util::GetKeyCode(m_Config.Get("key_reply_sender"));
  m_KeyForward = Util::GetKeyCode(m_Config.Get("key_forward"));
  m_KeyForwardAttached = Util::GetKeyCode(m_Config.Get("key_forward_attached"));
  m_KeyDelete = Util::GetKeyCode(m_Config.Get("key_delete"));
  m_KeyCompose = Util::GetKeyCode(m_Config.Get("key_compose"));
  m_KeyComposeCopy = Util::GetKeyCode(m_Config.Get("key_compose_copy"));
  m_KeyToggleUnread = Util::GetKeyCode(m_Config.Get("key_toggle_unread"));
  m_KeyMove = Util::GetKeyCode(m_Config.Get("key_move"));
  m_KeyAutoMove = Util::GetKeyCode(m_Config.Get("key_auto_move"));
  m_KeyRefresh = Util::GetKeyCode(m_Config.Get("key_refresh"));
  m_KeyQuit = Util::GetKeyCode(m_Config.Get("key_quit"));
  m_KeyToggleTextHtml = Util::GetKeyCode(m_Config.Get("key_toggle_text_html"));
  m_KeyCancel = Util::GetKeyCode(m_Config.Get("key_cancel"));
  m_KeySend = Util::GetKeyCode(m_Config.Get("key_send"));
  m_KeyDeleteCharAfterCursor = Util::GetKeyCode(m_Config.Get("key_delete_char_after_cursor"));
  m_KeyDeleteLineAfterCursor = Util::GetKeyCode(m_Config.Get("key_delete_line_after_cursor"));
  m_KeyDeleteLineBeforeCursor = Util::GetKeyCode(m_Config.Get("key_delete_line_before_cursor"));
  m_KeyOpen = Util::GetKeyCode(m_Config.Get("key_open"));
  m_KeyBack = Util::GetKeyCode(m_Config.Get("key_back"));
  m_KeyGotoFolder = Util::GetKeyCode(m_Config.Get("key_goto_folder"));
  m_KeyGotoInbox = Util::GetKeyCode(m_Config.Get("key_goto_inbox"));
  m_KeyToSelect = Util::GetKeyCode(m_Config.Get("key_to_select"));
  m_KeySaveFile = Util::GetKeyCode(m_Config.Get("key_save_file"));
  m_KeyExtEditor = Util::GetKeyCode(m_Config.Get("key_ext_editor"));
  m_KeyExtPager = Util::GetKeyCode(m_Config.Get("key_ext_pager"));
  m_KeyPostpone = Util::GetKeyCode(m_Config.Get("key_postpone"));
  m_KeyOtherCmdHelp = Util::GetKeyCode(m_Config.Get("key_othercmd_help"));
  m_KeyExport = Util::GetKeyCode(m_Config.Get("key_export"));
  m_KeyImport = Util::GetKeyCode(m_Config.Get("key_import"));
  m_KeyRichHeader = Util::GetKeyCode(m_Config.Get("key_rich_header"));
  m_KeyExtHtmlViewer = Util::GetKeyCode(m_Config.Get("key_ext_html_viewer"));
  m_KeyExtHtmlPreview = Util::GetKeyCode(m_Config.Get("key_ext_html_preview"));
  m_KeyExtMsgViewer = Util::GetKeyCode(m_Config.Get("key_ext_msg_viewer"));
  m_KeySearch = Util::GetKeyCode(m_Config.Get("key_search"));
  m_KeyFind = Util::GetKeyCode(m_Config.Get("key_find"));
  m_KeyFindNext = Util::GetKeyCode(m_Config.Get("key_find_next"));
  m_KeySync = Util::GetKeyCode(m_Config.Get("key_sync"));
  m_KeyToggleMarkdownCompose = Util::GetKeyCode(m_Config.Get("key_toggle_markdown_compose"));

  m_KeyBackwardWord = Util::GetKeyCode(m_Config.Get("key_backward_word"));
  m_KeyForwardWord = Util::GetKeyCode(m_Config.Get("key_forward_word"));
  m_KeyBackwardKillWord = Util::GetKeyCode(m_Config.Get("key_backward_kill_word"));
  m_KeyKillWord = Util::GetKeyCode(m_Config.Get("key_kill_word"));

  m_KeyBeginLine = Util::GetKeyCode(m_Config.Get("key_begin_line"));
  m_KeyEndLine = Util::GetKeyCode(m_Config.Get("key_end_line"));
  m_KeyPrevPage = Util::GetKeyCode(m_Config.Get("key_prev_page"));
  m_KeyNextPage = Util::GetKeyCode(m_Config.Get("key_next_page"));
  m_KeyPrevPageCompose = Util::GetKeyCode(m_Config.Get("key_prev_page_compose"));
  m_KeyNextPageCompose = Util::GetKeyCode(m_Config.Get("key_next_page_compose"));
  m_KeyFilterSortReset = Util::GetKeyCode(m_Config.Get("key_filter_sort_reset"));
  m_KeyFilterShowUnread = Util::GetKeyCode(m_Config.Get("key_filter_show_unread"));
  m_KeyFilterShowHasAttachments = Util::GetKeyCode(m_Config.Get("key_filter_show_has_attachments"));
  m_KeyFilterShowCurrentDate = Util::GetKeyCode(m_Config.Get("key_filter_show_current_date"));
  m_KeyFilterShowCurrentName = Util::GetKeyCode(m_Config.Get("key_filter_show_current_name"));
  m_KeyFilterShowCurrentSubject = Util::GetKeyCode(m_Config.Get("key_filter_show_current_subject"));
  m_KeySortUnread = Util::GetKeyCode(m_Config.Get("key_sort_unread"));
  m_KeySortHasAttachments = Util::GetKeyCode(m_Config.Get("key_sort_has_attachments"));
  m_KeySortDate = Util::GetKeyCode(m_Config.Get("key_sort_date"));
  m_KeySortName = Util::GetKeyCode(m_Config.Get("key_sort_name"));
  m_KeySortSubject = Util::GetKeyCode(m_Config.Get("key_sort_subject"));
  m_KeyJumpTo = Util::GetKeyCode(m_Config.Get("key_jump_to"));
  m_KeySearchShowFolder = Util::GetKeyCode(m_Config.Get("key_search_show_folder"));
  m_KeySearchCurrentSubject = Util::GetKeyCode(m_Config.Get("key_search_current_subject"));
  m_KeySearchCurrentName = Util::GetKeyCode(m_Config.Get("key_search_current_name"));
  m_KeySpell = Util::GetKeyCode(m_Config.Get("key_spell"));

  m_ShowProgress = Util::ToInteger(m_Config.Get("show_progress"));
  m_NewMsgBell = m_Config.Get("new_msg_bell") == "1";
  m_QuitWithoutConfirm = m_Config.Get("quit_without_confirm") == "1";
  m_SendWithoutConfirm = m_Config.Get("send_without_confirm") == "1";
  m_CancelWithoutConfirm = m_Config.Get("cancel_without_confirm") == "1";
  m_PostponeWithoutConfirm = m_Config.Get("postpone_without_confirm") == "1";
  m_DeleteWithoutConfirm = m_Config.Get("delete_without_confirm") == "1";
  m_ShowEmbeddedImages = m_Config.Get("show_embedded_images") == "1";
  m_ShowRichHeader = m_Config.Get("show_rich_header") == "1";

  m_ColorsEnabled = m_Config.Get("colors_enabled") == "1";

  LOG_IF_NONZERO(pipe(m_Pipe));

  if (m_ColorsEnabled && !has_colors())
  {
    LOG_WARNING("terminal does not support colors");
    m_ColorsEnabled = false;
  }

  if (m_ColorsEnabled)
  {
    start_color();
    assume_default_colors(-1, -1);

    const std::string defaultQuotedFg = (COLORS > 8) ? "gray" : "";
    const std::string defaultHiddenFg = (COLORS > 8) ? "gray" : "";
    const std::string defaultSelectedFg = (COLORS > 8) ? "gray" : "";
    const std::map<std::string, std::string> defaultColorsConfig =
    {
      { "color_dialog_bg", "reverse" },
      { "color_dialog_fg", "reverse" },
      { "color_help_desc_bg", "" },
      { "color_help_desc_fg", "" },
      { "color_help_keys_bg", "reverse" },
      { "color_help_keys_fg", "reverse" },
      { "color_highlighted_text_bg", "reverse" },
      { "color_highlighted_text_fg", "reverse" },
      { "color_quoted_text_bg", "" },
      { "color_quoted_text_fg", defaultQuotedFg },
      { "color_hidden_file_bg", "" },
      { "color_hidden_file_fg", defaultHiddenFg },
      { "color_regular_text_bg", "" },
      { "color_regular_text_fg", "" },
      { "color_selected_item_bg", "" },
      { "color_selected_item_fg", defaultSelectedFg },
      { "color_top_bar_bg", "reverse" },
      { "color_top_bar_fg", "reverse" },
    };
    const std::string colorsConfigPath(Util::GetApplicationDir() + std::string("colors.conf"));
    Config colorsConfig = Config(colorsConfigPath, defaultColorsConfig);
    colorsConfig.LogParams();

    const int colorRegularTextFg = Util::GetColor(colorsConfig.Get("color_regular_text_fg"));
    const int colorRegularTextBg = Util::GetColor(colorsConfig.Get("color_regular_text_bg"));
    assume_default_colors(colorRegularTextFg, colorRegularTextBg);

    m_AttrsDialog = Util::GetColorAttrs(colorsConfig.Get("color_dialog_fg"), colorsConfig.Get("color_dialog_bg"));
    m_AttrsHelpDesc =
      Util::GetColorAttrs(colorsConfig.Get("color_help_desc_fg"), colorsConfig.Get("color_help_desc_bg"));
    m_AttrsHelpKeys =
      Util::GetColorAttrs(colorsConfig.Get("color_help_keys_fg"), colorsConfig.Get("color_help_keys_bg"));
    m_AttrsHighlightedText =
      Util::GetColorAttrs(colorsConfig.Get("color_highlighted_text_fg"), colorsConfig.Get("color_highlighted_text_bg"));
    m_AttrsQuotedText =
      Util::GetColorAttrs(colorsConfig.Get("color_quoted_text_fg"), colorsConfig.Get("color_quoted_text_bg"));
    m_AttrsHiddenFile =
      Util::GetColorAttrs(colorsConfig.Get("color_hidden_file_fg"), colorsConfig.Get("color_hidden_file_bg"));
    m_AttrsTopBar = Util::GetColorAttrs(colorsConfig.Get("color_top_bar_fg"), colorsConfig.Get("color_top_bar_bg"));
    m_AttrsSelectedItem =
      Util::GetColorAttrs(colorsConfig.Get("color_selected_item_fg"), colorsConfig.Get("color_selected_item_bg"));

    if (m_AttrsHighlightedText == A_REVERSE)
    {
      m_AttrsSelectedHighlighted = m_AttrsSelectedItem | A_REVERSE;
    }
    else
    {
      m_AttrsSelectedHighlighted =
        Util::GetColorAttrs(colorsConfig.Get("color_selected_item_fg"), colorsConfig.Get("color_highlighted_text_bg"));
    }

    colorsConfig.Save();
    
    // Initialize beautiful colors for enhanced UI
    InitBeautifulColors();
  }

  m_AttachmentIndicator = m_Config.Get("attachment_indicator");
  m_BottomReply = m_Config.Get("bottom_reply") == "1";
  m_PersistSortFilter = m_Config.Get("persist_sortfilter") == "1";
  m_PersistSelectionOnSortFilterChange = m_Config.Get("persist_selection_on_sortfilter_change") == "1";
  m_UnreadIndicator = m_Config.Get("unread_indicator");
  m_InvalidInputNotify = m_Config.Get("invalid_input_notify") == "1";
  m_KeyToggleFullHeader = Util::GetKeyCode(m_Config.Get("key_toggle_full_header"));
  m_FullHeaderIncludeLocal = m_Config.Get("full_header_include_local") == "1";
  m_TabSize = Util::Bound(1, (int)Util::ToInteger(m_Config.Get("tab_size")), 80);
  m_KeySelectItem = Util::GetKeyCode(m_Config.Get("key_select_item"));
  m_KeySelectAll = Util::GetKeyCode(m_Config.Get("key_select_all"));
  m_SearchShowFolder = m_Config.Get("search_show_folder") == "1";
  Util::SetLocalizedSubjectPrefixes(m_Config.Get("localized_subject_prefixes"));
  m_Signature = m_Config.Get("signature") == "1";
  m_TopBarShowVersion = m_Config.Get("top_bar_show_version") == "1";

  try
  {
    m_ComposeBackupInterval = std::stoll(m_Config.Get("compose_backup_interval"));
  }
  catch (...)
  {
  }

  m_Status.SetShowProgress(m_ShowProgress);

  SetRunning(true);

  m_SleepDetect.reset(new SleepDetect(std::bind(&Ui::OnWakeUp, this), 10));
  
  // Initialize HTML parser
  m_HtmlParser.reset(new HtmlParser());
}

void Ui::Cleanup()
{
  m_SleepDetect.reset();

  m_Config.Set("plain_text", m_Plaintext ? "1" : "0");
  m_Config.Set("show_rich_header", m_ShowRichHeader ? "1" : "0");
  m_Config.Set("search_show_folder", m_SearchShowFolder ? "1" : "0");
  m_Config.Save();
  close(m_Pipe[0]);
  close(m_Pipe[1]);
  wclear(stdscr);
  endwin();

  if (!m_TerminalTitle.empty())
  {
    printf("\033]0;%s\007", "");
  }
}

void Ui::InitWindows()
{
  getmaxyx(stdscr, m_ScreenHeight, m_ScreenWidth);
  m_ScreenWidth = std::max(m_ScreenWidth, 40);
  m_ScreenHeight = std::max(m_ScreenHeight, 8);

  m_MaxViewLineLength = m_ScreenWidth;
  m_MaxComposeLineLength = (m_ComposeLineWrap == LineWrapHardWrap) ? std::min(m_ScreenWidth, 72) : m_ScreenWidth;
  wclear(stdscr);
  wrefresh(stdscr);
  const int topHeight = 1;
  m_TopWin = newwin(topHeight, m_ScreenWidth, 0, 0);
  leaveok(m_TopWin, true);

  int helpHeight = 0;
  if (m_HelpEnabled)
  {
    helpHeight = 2;
    m_HelpWin = newwin(2, m_ScreenWidth, m_ScreenHeight - helpHeight, 0);
    leaveok(m_HelpWin, true);
  }

  const int dialogHeight = 1;
  m_DialogWin = newwin(1, m_ScreenWidth, m_ScreenHeight - helpHeight - dialogHeight, 0);
  leaveok(m_DialogWin, true);

  bool listPad = true;
  if (listPad)
  {
    m_MainWinHeight = m_ScreenHeight - topHeight - helpHeight - 2;
    m_MainWin = newwin(m_MainWinHeight, m_ScreenWidth, topHeight + 1, 0);
  }
  else
  {
    m_MainWinHeight = m_ScreenHeight - topHeight - helpHeight;
    m_MainWin = newwin(m_MainWinHeight, m_ScreenWidth, topHeight, 0);
  }

  leaveok(m_MainWin, true);
  
  // Initialize HTML parser with screen dimensions
  if (m_HtmlParser) {
    m_HtmlParser->SetTerminalWidth(m_ScreenWidth);
  }
}

void Ui::CleanupWindows()
{
  delwin(m_TopWin);
  m_TopWin = NULL;
  delwin(m_MainWin);
  m_MainWin = NULL;
  delwin(m_DialogWin);
  m_DialogWin = NULL;
  if (m_HelpWin != NULL)
  {
    delwin(m_HelpWin);
    m_HelpWin = NULL;
  }
}

void Ui::DrawAll()
{
  switch (m_State)
  {
    case StateViewMessageList:
      DrawTop();
      if (m_MessageListSearch)
      {
        DrawMessageListSearch();
      }
      else
      {
        DrawMessageList();
      }
      DrawHelp();
      DrawDialog();
      break;

    case StateViewMessage:
      DrawTop();
      DrawMessage();
      DrawHelp();
      DrawDialog();
      break;

    case StateGotoFolder:
    case StateMoveToFolder:
      DrawTop();
      DrawFolderList();
      DrawHelp();
      DrawDialog();
      break;

    case StateComposeMessage:
    case StateComposeCopyMessage:
    case StateReplyAllMessage:
    case StateReplySenderMessage:
    case StateForwardMessage:
    case StateForwardAttachedMessage:
      DrawTop();
      DrawHelp();
      DrawDialog();
      DrawComposeMessage();
      break;

    case StateAddressList:
    case StateFromAddressList:
      DrawTop();
      DrawAddressList();
      DrawHelp();
      DrawDialog();
      break;

    case StateFileList:
      DrawTop();
      DrawFileList();
      DrawHelp();
      DrawDialog();
      break;

    case StateViewPartList:
      DrawTop();
      DrawPartList();
      DrawHelp();
      DrawDialog();
      break;

    default:
      werase(m_MainWin);
      
      // Set background color for default state
      if (m_ColorsEnabled)
      {
        wbkgd(m_MainWin, m_BeautifulColors[COLOR_MESSAGE_BACKGROUND]);
      }
      
      mvwprintw(m_MainWin, 0, 0, "Unimplemented state %d", m_State);
      wrefresh(m_MainWin);
      break;
  }
}

void Ui::DrawTop()
{
  // Use beautiful top bar if colors are enabled
  if (m_ColorsEnabled) {
    DrawBeautifulTopBar();
    return;
  }
  
  // Fallback to original design for non-color terminals
  werase(m_TopWin);
  wattron(m_TopWin, m_AttrsTopBar);

  static const std::string appName = "  " + Version::GetAppName(m_TopBarShowVersion);
  std::string topLeft = Util::TrimPadString(appName, (m_ScreenWidth - 13) / 2);
  std::string status = GetStatusStr();
  std::string topRight = status + "  ";
  int centerWidth = m_ScreenWidth - (int)topLeft.size() - (int)topRight.size() - 2;
  std::wstring wtopCenter = Util::TrimPadWString(Util::ToWString(GetStateStr()), centerWidth) + L"  ";
  std::string topCenter = Util::ToString(wtopCenter);
  std::string topCombined = topLeft + topCenter + topRight;

  mvwprintw(m_TopWin, 0, 0, "%s", topCombined.c_str());
  wattroff(m_TopWin, m_AttrsTopBar);
  wrefresh(m_TopWin);
}

void Ui::DrawDialog()
{
  switch (m_State)
  {
    case StateGotoFolder:
    case StateMoveToFolder:
    case StateAddressList:
    case StateFromAddressList:
    case StateFileList:
      DrawSearchDialog();
      break;

    default:
      DrawDefaultDialog();
      break;
  }
}

void Ui::DrawSearchDialog()
{
  int filterPos = 0;
  std::wstring filterStr;

  switch (m_State)
  {
    case StateGotoFolder:
    case StateMoveToFolder:
      filterPos = m_FolderListFilterPos;
      filterStr = m_FolderListFilterStr;
      break;

    case StateAddressList:
    case StateFromAddressList:
      filterPos = m_AddressListFilterPos;
      filterStr = m_AddressListFilterStr;
      break;

    case StateFileList:
      filterPos = m_FileListFilterPos;
      filterStr = m_FileListFilterStr;
      break;

    default:
      break;
  }

  werase(m_DialogWin);
  const std::string& dispStr = Util::ToString(filterStr);
  mvwprintw(m_DialogWin, 0, 0, "   Search: %s", dispStr.c_str());

  leaveok(m_DialogWin, false);
  wmove(m_DialogWin, 0, 11 + filterPos);
  wrefresh(m_DialogWin);
  leaveok(m_DialogWin, true);
}

void Ui::DrawDefaultDialog()
{
  // Don't erase if we have a beautiful status line that should persist
  bool hasBeautifulStatusLine = false;
  std::chrono::time_point<std::chrono::system_clock> nowTime =
    std::chrono::system_clock::now();
  std::chrono::duration<double> statusElapsed = nowTime - m_BeautifulStatusTime;
  
  // Check if we have a recent beautiful status line (within 30 seconds)
  if ((statusElapsed.count() < 30.0f) && !m_BeautifulStatusMessage.empty())
  {
    hasBeautifulStatusLine = true;
  }

  // Only show temporary dialog messages if no beautiful status line is active
  bool showDialogMessage = false;
  {
    std::lock_guard<std::mutex> lock(m_Mutex);
    std::chrono::duration<double> elapsed = nowTime - m_DialogMessageTime;
    if ((elapsed.count() < 0.5f) && !m_DialogMessage.empty())
    {
      showDialogMessage = true;
      hasBeautifulStatusLine = false; // Dialog message takes priority
    }
  }

  if (!hasBeautifulStatusLine)
  {
    werase(m_DialogWin);
  }

  if (showDialogMessage)
  {
    std::lock_guard<std::mutex> lock(m_Mutex);
    int x = std::max((m_ScreenWidth - (int)m_DialogMessage.size() - 1) / 2, 0);
    const std::string& dispStr = m_DialogMessage;
    wattron(m_DialogWin, m_AttrsDialog);
    mvwprintw(m_DialogWin, 0, x, " %s ", dispStr.c_str());
    wattroff(m_DialogWin, m_AttrsDialog);
  }

  wrefresh(m_DialogWin);
}

void Ui::SetDialogMessage(const std::string& p_DialogMessage, bool p_Warn /*= false */)
{
  std::lock_guard<std::mutex> lock(m_Mutex);
  m_DialogMessage = p_DialogMessage;
  m_DialogMessageTime = std::chrono::system_clock::now();
  if (!p_DialogMessage.empty())
  {
    const std::string& logMessage = Util::ToLower(p_DialogMessage);
    if (p_Warn)
    {
      LOG_WARNING("%s", logMessage.c_str());
    }
    else
    {
      LOG_DEBUG("%s", logMessage.c_str());
    }
  }
}

void Ui::DrawHelp()
{
  static std::vector<std::vector<std::string>> viewMessagesListCommonHelp =
  {
    {
      GetKeyDisplay(m_KeyBack), "Folders",
      GetKeyDisplay(m_KeyPrevMsg), "PrevMsg",
      GetKeyDisplay(m_KeyReplyAll), "Reply",
      GetKeyDisplay(m_KeyDelete), "Delete",
      GetKeyDisplay(m_KeyRefresh), "Refresh",
      GetKeyDisplay(m_KeyOtherCmdHelp), "OtherCmds",
    },
    {
      GetKeyDisplay(m_KeyOpen), "ViewMsg",
      GetKeyDisplay(m_KeyNextMsg), "NextMsg",
      GetKeyDisplay(m_KeyForward), "Forward",
      GetKeyDisplay(m_KeyCompose), "Compose",
      GetKeyDisplay(m_KeyAutoMove), "Move",
      GetKeyDisplay(m_KeyQuit), "Quit",
    },
    {
      GetKeyDisplay(m_KeyToggleUnread), "TgUnread",
      GetKeyDisplay(m_KeyExport), "Export",
      GetKeyDisplay(m_KeyImport), "Import",
      GetKeyDisplay(m_KeySearch), "Search",
      GetKeyDisplay(m_KeySync), "FullSync",
      GetKeyDisplay(m_KeyOtherCmdHelp), "OtherCmds",
    },
    {
      GetKeyDisplay(m_KeyExtHtmlViewer), "ExtVHtml",
      GetKeyDisplay(m_KeyExtMsgViewer), "ExtVMsg",
      GetKeyDisplay(m_KeySelectAll), "SelectAll",
      GetKeyDisplay(m_KeyGotoInbox), "GotoInbox",
      GetKeyDisplay(m_KeySearchCurrentSubject), "SearcSubj",
      GetKeyDisplay(m_KeySearchCurrentName), "SearcName",
    },
  };

  static std::vector<std::vector<std::string>> viewMessagesListHelp = [&]()
  {
    std::vector<std::vector<std::string>> listHelp = viewMessagesListCommonHelp;
    listHelp.push_back(
    {
      GetKeyDisplay(m_KeySortUnread), "SortUnrd",
      GetKeyDisplay(m_KeySortHasAttachments), "SortAttc",
      GetKeyDisplay(m_KeySortDate), "SortDate",
      GetKeyDisplay(m_KeySortName), "SortName",
      GetKeyDisplay(m_KeySortSubject), "SortSubj",
      GetKeyDisplay(m_KeyOtherCmdHelp), "OtherCmds",
    });
    listHelp.push_back(
    {
      GetKeyDisplay(m_KeyFilterShowUnread), "FiltUnrd",
      GetKeyDisplay(m_KeyFilterShowHasAttachments), "FiltAttc",
      GetKeyDisplay(m_KeyFilterShowCurrentDate), "FiltDate",
      GetKeyDisplay(m_KeyFilterShowCurrentName), "FiltName",
      GetKeyDisplay(m_KeyFilterShowCurrentSubject), "FiltSubj",
      GetKeyDisplay(m_KeyFilterSortReset), "FiltReset",
    });
    return listHelp;
  }();

  static std::vector<std::vector<std::string>> viewMessagesListSearchHelp = [&]()
  {
    std::vector<std::vector<std::string>> listHelp = viewMessagesListCommonHelp;
    listHelp[0][1] = "MsgList"; // instead of "Folders"

    listHelp.push_back(
    {
      GetKeyDisplay(m_KeyJumpTo), "JumpTo",
      "", "",
      "", "",
      "", "",
      "", "",
      GetKeyDisplay(m_KeyOtherCmdHelp), "OtherCmds",
    });
    listHelp.push_back(
    {
      GetKeyDisplay(m_KeySearchShowFolder), "ShowFold",
    });

    return listHelp;
  }();

  static std::vector<std::vector<std::string>> viewMessageHelp =
  {
    {
      GetKeyDisplay(m_KeyBack), "MsgList",
      GetKeyDisplay(m_KeyPrevMsg), "PrevMsg",
      GetKeyDisplay(m_KeyReplyAll), "Reply",
      GetKeyDisplay(m_KeyDelete), "Delete",
      GetKeyDisplay(m_KeyToggleTextHtml), "TgTxtHtml",
      GetKeyDisplay(m_KeyOtherCmdHelp), "OtherCmds",
    },
    {
      GetKeyDisplay(m_KeyOpen), "MsgParts",
      GetKeyDisplay(m_KeyNextMsg), "NextMsg",
      GetKeyDisplay(m_KeyForward), "Forward",
      GetKeyDisplay(m_KeyCompose), "Compose",
      GetKeyDisplay(m_KeyAutoMove), "Move",
      GetKeyDisplay(m_KeyQuit), "Quit",
    },
    {
      GetKeyDisplay(m_KeyToggleUnread), "TgUnread",
      GetKeyDisplay(m_KeyExport), "Export",
      GetKeyDisplay(m_KeyExtPager), "ExtPager",
      GetKeyDisplay(m_KeyExtHtmlViewer), "ExtVHtml",
      GetKeyDisplay(m_KeyExtMsgViewer), "ExtVMsg",
      GetKeyDisplay(m_KeyOtherCmdHelp), "OtherCmds",
    },
    {
      GetKeyDisplay(m_KeyFind), "Find",
      GetKeyDisplay(m_KeyFindNext), "FindNext",
      GetKeyDisplay(m_KeyToggleFullHeader), "TgFullHdr",
      GetKeyDisplay(m_KeyGotoInbox), "GotoInbox",
    },
  };

  static std::vector<std::vector<std::string>> viewFoldersHelp =
  {
    {
      GetKeyDisplay(KEY_RETURN), "Select",
    },
    {
      GetKeyDisplay(m_KeyCancel), "Cancel",
    }
  };

  static std::vector<std::vector<std::string>> composeMessageHelp =
  {
    {
      GetKeyDisplay(m_KeySend), "Send",
      GetKeyDisplay(m_KeyExtEditor), "ExtEdit",
      GetKeyDisplay(m_KeyRichHeader), "RichHdr",
      GetKeyDisplay(m_KeyToggleMarkdownCompose), "TgMkDown",
      GetKeyDisplay(m_KeySpell), "Spell",
    },
    {
      GetKeyDisplay(m_KeyCancel), "Cancel",
      GetKeyDisplay(m_KeyPostpone), "Postpone",
      GetKeyDisplay(m_KeyToSelect), "ToSelect",
      GetKeyDisplay(m_KeyExtHtmlPreview), "ExtVHtml",
    },
  };

  static std::vector<std::vector<std::string>> viewPartListHelp =
  {
    {
      GetKeyDisplay(m_KeyBack), "ViewMsg",
      GetKeyDisplay(m_KeyPrevMsg), "PrevPart",
      GetKeyDisplay(m_KeySaveFile), "Save",
      GetKeyDisplay(m_KeyGotoInbox), "GotoInbox",
    },
    {
      GetKeyDisplay(m_KeyOpen), "ViewPart",
      GetKeyDisplay(m_KeyNextMsg), "NextPart",
      GetKeyDisplay(m_KeyQuit), "Quit",
    },
  };

  if (m_HelpEnabled)
  {
    werase(m_HelpWin);
    switch (m_State)
    {
      case StateViewMessageList:
        {
          const std::vector<std::vector<std::string>>& listHelp =
            m_MessageListSearch ? viewMessagesListSearchHelp : viewMessagesListHelp;
          m_HelpViewMessagesListSize = listHelp.size();

          auto first = listHelp.begin() + m_HelpViewMessagesListOffset;
          auto last = listHelp.begin() + m_HelpViewMessagesListOffset + 2;
          std::vector<std::vector<std::string>> helpMessages(first, last);
          DrawHelpText(helpMessages);
          break;
        }

      case StateViewMessage:
        {
          auto first = viewMessageHelp.begin() + m_HelpViewMessageOffset;
          auto last = viewMessageHelp.begin() + m_HelpViewMessageOffset + 2;
          std::vector<std::vector<std::string>> helpMessages(first, last);
          DrawHelpText(helpMessages);
          break;
        }
        break;

      case StateGotoFolder:
      case StateMoveToFolder:
      case StateAddressList:
      case StateFromAddressList:
      case StateFileList:
        DrawHelpText(viewFoldersHelp);
        break;

      case StateComposeMessage:
      case StateComposeCopyMessage:
      case StateReplyAllMessage:
      case StateReplySenderMessage:
      case StateForwardMessage:
      case StateForwardAttachedMessage:
        DrawHelpText(composeMessageHelp);
        break;

      case StateViewPartList:
        DrawHelpText(viewPartListHelp);
        break;

      default:
        break;
    }

    wrefresh(m_HelpWin);
  }
}

void Ui::DrawHelpText(const std::vector<std::vector<std::string>>& p_HelpText)
{
  int cols = 6;
  int width = m_ScreenWidth / cols;

  int y = 0;
  for (auto rowIt = p_HelpText.begin(); rowIt != p_HelpText.end(); ++rowIt)
  {
    int x = 0;
    for (int colIdx = 0; colIdx < (int)rowIt->size(); colIdx += 2)
    {
      std::wstring wcmd = Util::ToWString(rowIt->at(colIdx));
      std::wstring wdesc = Util::ToWString(rowIt->at(colIdx + 1));

      wattron(m_HelpWin, m_AttrsHelpKeys);
      mvwaddnwstr(m_HelpWin, y, x, wcmd.c_str(), wcmd.size());
      wattroff(m_HelpWin, m_AttrsHelpKeys);

      wattron(m_HelpWin, m_AttrsHelpDesc);
      const std::wstring wdescTrim = wdesc.substr(0, width - wcmd.size() - 2);
      mvwaddnwstr(m_HelpWin, y, x + wcmd.size() + 1, wdescTrim.c_str(), wdescTrim.size());
      wattroff(m_HelpWin, m_AttrsHelpDesc);

      x += width;
    }

    ++y;
  }
}

void Ui::DrawFolderList()
{
  if (!m_HasRequestedFolders)
  {
    ImapManager::Request request;
    request.m_GetFolders = true;
    LOG_DEBUG("async req folders");
    m_HasRequestedFolders = true;
    m_ImapManager->AsyncRequest(request);
  }

  werase(m_MainWin);
  
  // Set background color for the entire folder list area
  if (m_ColorsEnabled)
  {
    wbkgd(m_MainWin, m_BeautifulColors[COLOR_MESSAGE_BACKGROUND]);
  }

  std::set<std::string> folders;

  bool hasFolders = false;
  if (m_FolderListFilterStr.empty())
  {
    std::lock_guard<std::mutex> lock(m_Mutex);
    hasFolders = !m_Folders.empty();
    folders = m_Folders;
  }
  else
  {
    std::lock_guard<std::mutex> lock(m_Mutex);
    hasFolders = !m_Folders.empty();
    for (const auto& folder : m_Folders)
    {
      if (Util::ToLower(folder).find(Util::ToLower(Util::ToString(m_FolderListFilterStr)))
          != std::string::npos)
      {
        folders.insert(folder);
      }
    }
  }

  int count = folders.size();
  if (count > 0)
  {
    if (m_FolderListCurrentIndex == INT_MAX)
    {
      for (int i = 0; i < count; ++i)
      {
        const std::string& folder = *std::next(folders.begin(), i);
        if (folder == m_FolderListCurrentFolder)
        {
          m_FolderListCurrentIndex = i;
        }
      }
    }

    m_FolderListCurrentIndex = Util::Bound(0, m_FolderListCurrentIndex, (int)folders.size() - 1);

    int itemsMax = m_MainWinHeight - 1;
    int idxOffs = Util::Bound(0, (int)(m_FolderListCurrentIndex - ((itemsMax - 1) / 2)),
                              std::max(0, (int)folders.size() - (int)itemsMax));
    int idxMax = idxOffs + std::min(itemsMax, (int)folders.size());

    // Beautiful folder list rendering
    for (int i = idxOffs; i < idxMax; ++i)
    {
      const std::string& folder = *std::next(folders.begin(), i);
      bool isCurrent = (i == m_FolderListCurrentIndex);

      if (isCurrent)
      {
        m_FolderListCurrentFolder = folder;
        wattron(m_MainWin, m_BeautifulColors[COLOR_ACTIVE_ITEM]);
      }
      else
      {
        wattron(m_MainWin, m_BeautifulColors[COLOR_FOLDER_ITEM]);
      }

      // Add beautiful folder icons based on folder type
      std::string folderIcon = GetUnicodeSymbol(SYMBOL_FOLDER);
      std::string lowerFolder = Util::ToLower(folder);
      
      if (lowerFolder.find("inbox") != std::string::npos)
      {
        folderIcon = GetUnicodeSymbol(SYMBOL_INBOX);
      }
      else if (lowerFolder.find("sent") != std::string::npos || 
               lowerFolder.find("outbox") != std::string::npos)
      {
        folderIcon = GetUnicodeSymbol(SYMBOL_SENT);
      }
      else if (lowerFolder.find("draft") != std::string::npos)
      {
        folderIcon = GetUnicodeSymbol(SYMBOL_DRAFT);
      }
      else if (lowerFolder.find("trash") != std::string::npos || 
               lowerFolder.find("deleted") != std::string::npos)
      {
        folderIcon = GetUnicodeSymbol(SYMBOL_TRASH);
      }
      else if (lowerFolder.find("spam") != std::string::npos || 
               lowerFolder.find("junk") != std::string::npos)
      {
        folderIcon = GetUnicodeSymbol(SYMBOL_SPAM);
      }
      else if (lowerFolder.find("archive") != std::string::npos)
      {
        folderIcon = GetUnicodeSymbol(SYMBOL_ARCHIVE);
      }

      // Beautiful folder display with icon and name
      int yPos = i - idxOffs;
      mvwaddstr(m_MainWin, yPos, 1, folderIcon.c_str());
      
      std::wstring wfolder = Util::ToWString(folder);
      mvwaddnwstr(m_MainWin, yPos, 3, wfolder.c_str(), wfolder.size());

      if (isCurrent)
      {
        wattroff(m_MainWin, m_BeautifulColors[COLOR_ACTIVE_ITEM]);
      }
      else
      {
        wattroff(m_MainWin, m_BeautifulColors[COLOR_FOLDER_ITEM]);
      }
    }
  }
  else
  {
    if (hasFolders)
    {
      m_FolderListCurrentFolder = "";
    }
  }

  // Beautiful status line for folder list
  std::string folderListInfo = "Select folder - Use arrow keys to navigate";
  DrawBeautifulStatusLine(folderListInfo, "folders");

  wrefresh(m_MainWin);
}

void Ui::DrawAddressList()
{
  werase(m_MainWin);
  
  // Set background color for the entire address list area
  if (m_ColorsEnabled)
  {
    wbkgd(m_MainWin, m_BeautifulColors[COLOR_MESSAGE_BACKGROUND]);
  }

  static std::wstring lastAddressListFilterStr = m_AddressListFilterStr;
  if (m_AddressListFilterStr != lastAddressListFilterStr)
  {
    std::string filter = Util::ToString(m_AddressListFilterStr);
    m_Addresses = (m_State == StateAddressList) ? AddressBook::Get(filter)
                                                : AddressBook::GetFrom(filter);
    lastAddressListFilterStr = m_AddressListFilterStr;
  }

  int count = m_Addresses.size();
  if (count > 0)
  {
    m_AddressListCurrentIndex = Util::Bound(0, m_AddressListCurrentIndex, (int)m_Addresses.size() - 1);

    int itemsMax = m_MainWinHeight - 1;
    int idxOffs = Util::Bound(0, (int)(m_AddressListCurrentIndex - ((itemsMax - 1) / 2)),
                              std::max(0, (int)m_Addresses.size() - (int)itemsMax));
    int idxMax = idxOffs + std::min(itemsMax, (int)m_Addresses.size());

    for (int i = idxOffs; i < idxMax; ++i)
    {
      const std::string& address = *std::next(m_Addresses.begin(), i);

      if (i == m_AddressListCurrentIndex)
      {
        wattron(m_MainWin, m_AttrsHighlightedText);
        m_AddressListCurrentAddress = address;
      }

      std::wstring waddress = Util::ToWString(address);
      size_t maxWidth = m_ScreenWidth - 4;
      if (waddress.size() > maxWidth)
      {
        static const std::wstring suffix = L"...";
        waddress = waddress.substr(0, maxWidth - suffix.size()) + suffix;
      }

      mvwaddnwstr(m_MainWin, i - idxOffs, 2, waddress.c_str(), waddress.size());

      if (i == m_AddressListCurrentIndex)
      {
        wattroff(m_MainWin, m_AttrsHighlightedText);
      }
    }
  }

  wrefresh(m_MainWin);
}

void Ui::DrawFileList()
{
  werase(m_MainWin);
  
  // Set background color for the entire file list area
  if (m_ColorsEnabled)
  {
    wbkgd(m_MainWin, m_BeautifulColors[COLOR_MESSAGE_BACKGROUND]);
  }

  std::set<Fileinfo, FileinfoCompare> files;

  if (m_FileListFilterStr.empty())
  {
    files = m_Files;
  }
  else
  {
    for (const auto& file : m_Files)
    {
      if (Util::ToLower(file.m_Name).find(Util::ToLower(Util::ToString(m_FileListFilterStr)))
          != std::string::npos)
      {
        files.insert(file);
      }
    }
  }

  int maxWidth = (m_ScreenWidth - 4);
  std::wstring dirLabel = L"Dir: ";
  int maxDirLen = maxWidth - dirLabel.size();
  std::wstring dirPath = Util::ToWString(m_CurrentDir);
  int dirPathRight = ((int)dirPath.size() < maxDirLen) ? 0 : (dirPath.size() - maxDirLen);
  dirLabel += dirPath.substr(dirPathRight);
  mvwaddnwstr(m_MainWin, 0, 2, dirLabel.c_str(), dirLabel.size());

  int count = files.size();
  if (count > 0)
  {
    m_FileListCurrentIndex = Util::Bound(0, m_FileListCurrentIndex, (int)files.size() - 1);

    int posOffs = 2;
    int itemsMax = m_MainWinHeight - 1 - posOffs;
    int idxOffs = Util::Bound(0, (int)(m_FileListCurrentIndex - ((itemsMax - 1) / 2)),
                              std::max(0, (int)files.size() - (int)itemsMax));
    int idxMax = idxOffs + std::min(itemsMax, (int)files.size());

    for (int i = idxOffs; i < idxMax; ++i)
    {
      const Fileinfo& fileinfo = *std::next(files.begin(), i);

      int maxNameLen = maxWidth - 2 - 7; // two spaces and (dir) / 1023 KB
      std::wstring name = Util::ToWString(fileinfo.m_Name);
      name = Util::TrimPadWString(name.substr(0, maxNameLen), maxNameLen);

      if (fileinfo.IsDir())
      {
        name += L"    (dir)";
      }
      else
      {
        // max 7 - ex: "1023 KB"
        std::string size = Util::GetPrefixedSize(fileinfo.m_Size);
        size = std::string(7 - size.size(), ' ') + size;
        name += L"  " + Util::ToWString(size);
      }

      const bool isHidden = fileinfo.IsHidden();
      if (isHidden)
      {
        wattron(m_MainWin, m_AttrsHiddenFile);
      }

      if (i == m_FileListCurrentIndex)
      {
        wattron(m_MainWin, m_AttrsHighlightedText);
        m_FileListCurrentFile = fileinfo;
      }

      mvwaddnwstr(m_MainWin, i - idxOffs + posOffs, 2, name.c_str(), name.size());

      if (i == m_FileListCurrentIndex)
      {
        wattroff(m_MainWin, m_AttrsHighlightedText);
      }

      if (isHidden)
      {
        wattroff(m_MainWin, m_AttrsHiddenFile);
      }
    }
  }

  wrefresh(m_MainWin);
}

void Ui::DrawMessageList()
{
  if (!m_HasRequestedUids[m_CurrentFolder])
  {
    ImapManager::Request request;
    request.m_Folder = m_CurrentFolder;
    request.m_GetUids = true;
    LOG_DEBUG_VAR("async req uids =", m_CurrentFolder);
    m_HasRequestedUids[m_CurrentFolder] = true;
    m_ImapManager->AsyncRequest(request);
  }

  std::set<uint32_t> fetchHeaderUids;
  std::set<uint32_t> fetchFlagUids;
  std::set<uint32_t> fetchBodyPriUids;
  std::set<uint32_t> fetchBodySecUids;
  std::set<uint32_t> prefetchBodyUids;

  {
    std::lock_guard<std::mutex> lock(m_Mutex);
    std::map<uint32_t, Header>& headers = m_Headers[m_CurrentFolder];
    std::map<uint32_t, uint32_t>& flags = m_Flags[m_CurrentFolder];
    const std::map<std::string, uint32_t>& displayUids = GetDisplayUids(m_CurrentFolder);

    std::set<uint32_t>& requestedHeaders = m_RequestedHeaders[m_CurrentFolder];
    std::set<uint32_t>& requestedFlags = m_RequestedFlags[m_CurrentFolder];
    const std::map<uint32_t, Body>& bodys = m_Bodys[m_CurrentFolder];
    std::set<uint32_t>& prefetchedBodys = m_PrefetchedBodys[m_CurrentFolder];
    std::set<uint32_t>& requestedBodys = m_RequestedBodys[m_CurrentFolder];
    const std::string& currentDate = Header::GetCurrentDate();

    auto selectedUidsIt = m_SelectedUids.find(m_CurrentFolder);
    std::set<uint32_t> noSelection;
    const std::set<uint32_t>& folderSelectedUids =
      (selectedUidsIt != m_SelectedUids.end()) ? selectedUidsIt->second : noSelection;

    if (!m_PrefetchAllHeaders)
    {
      // get headers / flags for current view and next page
      int idxOffs = Util::Bound(0, (int)(m_MessageListCurrentIndex[m_CurrentFolder] -
                                         ((m_MainWinHeight - 1) / 2)),
                                std::max(0, (int)displayUids.size() - (int)m_MainWinHeight));
      int idxMax = std::min(idxOffs + (m_MainWinHeight * 2), (int)displayUids.size());
      for (int i = idxOffs; i < idxMax; ++i)
      {
        uint32_t uid = std::prev(displayUids.end(), i + 1)->second;

        if ((headers.find(uid) == headers.end()) &&
            (requestedHeaders.find(uid) == requestedHeaders.end()))
        {
          fetchHeaderUids.insert(uid);
          requestedHeaders.insert(uid);
        }

        if ((flags.find(uid) == flags.end()) &&
            (requestedFlags.find(uid) == requestedFlags.end()))
        {
          fetchFlagUids.insert(uid);
          requestedFlags.insert(uid);
        }
      }
    }

    werase(m_MainWin);
    
    // Set background color for the entire message list area
    if (m_ColorsEnabled)
    {
      wbkgd(m_MainWin, m_BeautifulColors[COLOR_MESSAGE_BACKGROUND]);
    }

    int idxOffs = Util::Bound(0, (int)(m_MessageListCurrentIndex[m_CurrentFolder] -
                                       ((m_MainWinHeight - 1) / 2)),
                              std::max(0, (int)displayUids.size() - (int)m_MainWinHeight));
    int idxMax = idxOffs + std::min(m_MainWinHeight, (int)displayUids.size());

    for (int i = idxOffs; i < idxMax; ++i)
    {
      uint32_t uid = std::prev(displayUids.end(), i + 1)->second;

      bool isUnread = ((flags.find(uid) != flags.end()) && (!Flag::GetSeen(flags.at(uid))));
      // Beautiful message item rendering with modern icons and colors
      std::string shortDate;
      std::string shortFrom;
      std::string subject;
      auto hit = headers.find(uid);
      if (hit != headers.end())
      {
        Header& header = headers.at(uid);
        shortDate = header.GetDateOrTime(currentDate);
        subject = header.GetSubject();
        if (m_CurrentFolder == m_SentFolder)
        {
          shortFrom = header.GetShortTo();
        }
        else
        {
          shortFrom = header.GetShortFrom();
        }
      }

      bool isSelected = (folderSelectedUids.find(uid) != folderSelectedUids.end());
      bool isCurrent = (i == m_MessageListCurrentIndex[m_CurrentFolder]);

      // Beautiful message line construction - with simple unread mark and date on right
      std::string messageText;
      
      // Add simple unread/read indicator
      std::string unreadMark = isUnread ? "●" : "○";
      
      // Format and color the message line
      shortDate = Util::TrimPadString(shortDate, 10);
      shortFrom = Util::ToString(Util::TrimPadWString(Util::ToWString(shortFrom), 18));
      
      // Calculate available width for subject (mark + from + padding + date on right)
      int markWidth = 2; // mark + space
      int dateWidth = 10;
      int fromWidth = 18;
      int padding = 6; // spaces between fields
      int subjectWidth = m_ScreenWidth - markWidth - fromWidth - dateWidth - padding - 2;
      
      subject = Util::ToString(Util::TrimPadWString(Util::ToWString(subject), subjectWidth));
      
      // Apply beautiful colors
      if (isCurrent)
      {
        wattron(m_MainWin, m_BeautifulColors[COLOR_ACTIVE_ITEM]);
      }
      else if (isSelected)
      {
        wattron(m_MainWin, m_BeautifulColors[COLOR_SELECTED_ITEM]);
      }
      else if (isUnread)
      {
        wattron(m_MainWin, m_BeautifulColors[COLOR_UNREAD_ITEM]);
      }
      else
      {
        wattron(m_MainWin, m_BeautifulColors[COLOR_NORMAL_ITEM]);
      }

      // Draw the message line with beautiful formatting
      int yPos = i - idxOffs;
      int xPos = 0;
      
      // Unread mark
      mvwaddstr(m_MainWin, yPos, xPos, unreadMark.c_str());
      xPos += markWidth;
      
      // From section with sender color
      wattron(m_MainWin, m_BeautifulColors[COLOR_SENDER_NAME]);
      mvwaddstr(m_MainWin, yPos, xPos, shortFrom.c_str());
      wattroff(m_MainWin, m_BeautifulColors[COLOR_SENDER_NAME]);
      xPos += fromWidth + 2;
      
      // Subject section
      wattron(m_MainWin, isCurrent ? m_BeautifulColors[COLOR_ACTIVE_SUBJECT] : 
                         isUnread ? m_BeautifulColors[COLOR_UNREAD_SUBJECT] : 
                         m_BeautifulColors[COLOR_SUBJECT_TEXT]);
      mvwaddstr(m_MainWin, yPos, xPos, subject.c_str());
      wattroff(m_MainWin, isCurrent ? m_BeautifulColors[COLOR_ACTIVE_SUBJECT] : 
                          isUnread ? m_BeautifulColors[COLOR_UNREAD_SUBJECT] : 
                          m_BeautifulColors[COLOR_SUBJECT_TEXT]);
      
      // Date section on the right with subtle color
      wattron(m_MainWin, m_BeautifulColors[COLOR_DATE_TIME]);
      mvwaddstr(m_MainWin, yPos, m_ScreenWidth - dateWidth - 1, shortDate.c_str());
      wattroff(m_MainWin, m_BeautifulColors[COLOR_DATE_TIME]);
      
      // Turn off main coloring
      if (isCurrent)
      {
        wattroff(m_MainWin, m_BeautifulColors[COLOR_ACTIVE_ITEM]);
      }
      else if (isSelected)
      {
        wattroff(m_MainWin, m_BeautifulColors[COLOR_SELECTED_ITEM]);
      }
      else if (isUnread)
      {
        wattroff(m_MainWin, m_BeautifulColors[COLOR_UNREAD_ITEM]);
      }
      else
      {
        wattroff(m_MainWin, m_BeautifulColors[COLOR_NORMAL_ITEM]);
      }

      if (i == m_MessageListCurrentIndex[m_CurrentFolder])
      {
        if ((bodys.find(uid) == bodys.end()) &&
            (requestedBodys.find(uid) == requestedBodys.end()))
        {
          if (m_PrefetchLevel >= PrefetchLevelCurrentMessage)
          {
            requestedBodys.insert(uid);
            fetchBodyPriUids.insert(uid);
          }
        }
      }
      else if (abs(i - m_MessageListCurrentIndex[m_CurrentFolder]) == 1)
      {
        if ((bodys.find(uid) == bodys.end()) &&
            (requestedBodys.find(uid) == requestedBodys.end()))
        {
          if (m_PrefetchLevel >= PrefetchLevelCurrentView)
          {
            requestedBodys.insert(uid);
            fetchBodySecUids.insert(uid);
          }
        }
      }
      else
      {
        if ((bodys.find(uid) == bodys.end()) &&
            (prefetchedBodys.find(uid) == prefetchedBodys.end()) &&
            (requestedBodys.find(uid) == requestedBodys.end()))
        {
          if (m_PrefetchLevel >= PrefetchLevelCurrentView)
          {
            prefetchedBodys.insert(uid);
            prefetchBodyUids.insert(uid);
          }
        }
      }
    }
  }

  for (auto& uid : fetchBodyPriUids)
  {
    ImapManager::Request request;
    request.m_Folder = m_CurrentFolder;

    std::set<uint32_t> fetchUids;
    fetchUids.insert(uid);
    request.m_GetBodys = fetchUids;
    request.m_ProcessHtml = !m_Plaintext;

    LOG_DEBUG_VAR("async req pri bodys =", fetchUids);
    m_ImapManager->AsyncRequest(request);
  }

  for (auto& uid : fetchBodySecUids)
  {
    ImapManager::Request request;
    request.m_Folder = m_CurrentFolder;

    std::set<uint32_t> fetchUids;
    fetchUids.insert(uid);
    request.m_GetBodys = fetchUids;
    request.m_ProcessHtml = !m_Plaintext;

    LOG_DEBUG_VAR("async req sec bodys =", fetchUids);
    m_ImapManager->AsyncRequest(request);
  }

  for (auto& uid : prefetchBodyUids)
  {
    ImapManager::Request request;
    request.m_PrefetchLevel = PrefetchLevelCurrentView;
    request.m_Folder = m_CurrentFolder;

    std::set<uint32_t> fetchUids;
    fetchUids.insert(uid);
    request.m_GetBodys = fetchUids;

    LOG_DEBUG_VAR("prefetch req bodys =", fetchUids);
    m_ImapManager->PrefetchRequest(request);
  }

  const int maxHeadersFetchRequest = 25;
  if (!fetchHeaderUids.empty())
  {
    LOG_DEBUG("fetching %d headers on demand", fetchHeaderUids.size());

    std::set<uint32_t> subsetFetchHeaderUids;
    for (auto it = fetchHeaderUids.begin(); it != fetchHeaderUids.end(); ++it)
    {
      subsetFetchHeaderUids.insert(*it);
      if ((subsetFetchHeaderUids.size() == maxHeadersFetchRequest) ||
          (std::next(it) == fetchHeaderUids.end()))
      {
        ImapManager::Request request;
        request.m_Folder = m_CurrentFolder;
        request.m_GetHeaders = subsetFetchHeaderUids;

        LOG_DEBUG_VAR("async req headers =", subsetFetchHeaderUids);
        m_ImapManager->AsyncRequest(request);

        subsetFetchHeaderUids.clear();
      }
    }
  }

  const int maxFlagsFetchRequest = 1000;
  if (!fetchFlagUids.empty())
  {
    LOG_DEBUG("fetching %d flags on demand", fetchFlagUids.size());

    std::set<uint32_t> subsetFetchFlagUids;
    for (auto it = fetchFlagUids.begin(); it != fetchFlagUids.end(); ++it)
    {
      subsetFetchFlagUids.insert(*it);
      if ((subsetFetchFlagUids.size() == maxFlagsFetchRequest) ||
          (std::next(it) == fetchFlagUids.end()))
      {
        ImapManager::Request request;
        request.m_Folder = m_CurrentFolder;
        request.m_GetFlags = subsetFetchFlagUids;

        LOG_DEBUG_VAR("async req flags =", subsetFetchFlagUids);
        m_ImapManager->AsyncRequest(request);

        subsetFetchFlagUids.clear();
      }
    }
  }

  // Beautiful status line for message list
  std::string folderInfo = "Folder: " + m_CurrentFolder;
  DrawBeautifulStatusLine(folderInfo, "folder");

  wrefresh(m_MainWin);
}

void Ui::DrawMessageListSearch()
{
  std::map<std::string, std::set<uint32_t>> fetchFlagUids;
  std::map<std::string, std::set<uint32_t>> fetchHeaderUids;
  std::map<std::string, std::set<uint32_t>> fetchBodyPriUids;
  std::map<std::string, std::set<uint32_t>> fetchBodySecUids;

  {
    std::lock_guard<std::mutex> searchLock(m_SearchMutex);
    std::vector<Header>& headers = m_MessageListSearchResultHeaders;
    int idxOffs = Util::Bound(0, (int)(m_MessageListCurrentIndex[m_CurrentFolder] - ((m_MainWinHeight - 1) / 2)),
                              std::max(0, (int)headers.size() - (int)m_MainWinHeight));
    int idxMax = idxOffs + std::min(m_MainWinHeight, (int)headers.size());
    const std::string& currentDate = Header::GetCurrentDate();

    werase(m_MainWin);
    
    // Set background color for the entire search results area
    if (m_ColorsEnabled)
    {
      wbkgd(m_MainWin, m_BeautifulColors[COLOR_MESSAGE_BACKGROUND]);
    }

    for (int i = idxOffs; i < idxMax; ++i)
    {
      const std::string& folder = m_MessageListSearchResultFolderUids.at(i).first;
      const int uid = m_MessageListSearchResultFolderUids.at(i).second;

      {
        std::lock_guard<std::mutex> lock(m_Mutex);

        std::map<uint32_t, uint32_t>& flags = m_Flags[folder];
        std::set<uint32_t>& requestedFlags = m_RequestedFlags[folder];
        if ((flags.find(uid) == flags.end()) &&
            (requestedFlags.find(uid) == requestedFlags.end()))
        {
          fetchFlagUids[folder].insert(uid);
          requestedFlags.insert(uid);
        }
      }

      // Check if message is unread
      bool isUnread = false;
      {
        std::lock_guard<std::mutex> lock(m_Mutex);
        std::map<uint32_t, uint32_t>& flags = m_Flags[folder];
        isUnread = ((flags.find(uid) != flags.end()) && (!Flag::GetSeen(flags.at(uid))));
      }

      std::string shortDate;
      std::string shortFrom;
      std::string subject;
      {
        Header& header = headers[i];
        shortDate = header.GetDateOrTime(currentDate);
        shortFrom = header.GetShortFrom();
        subject = header.GetSubject();
      }

      // Add simple unread/read indicator
      std::string unreadMark = isUnread ? "●" : "○";

      shortDate = Util::TrimPadString(shortDate, 10);
      shortFrom = Util::ToString(Util::TrimPadWString(Util::ToWString(shortFrom), 20));
      
      // Calculate layout with mark at start, date on right
      std::string folderTag = m_SearchShowFolder ? ("  [" + Util::BaseName(folder) + "]") : "";
      int markWidth = 2; // mark + space
      int dateWidth = 10;
      int fromWidth = 20;
      int padding = 4;
      int subjectWidth = m_ScreenWidth - markWidth - fromWidth - dateWidth - 
                        Util::WStringWidth(Util::ToWString(folderTag)) - padding - 2;
      
      subject = Util::ToString(Util::TrimPadWString(Util::ToWString(subject), subjectWidth));
      std::string header = unreadMark + " " + shortFrom + "  " + subject + folderTag;

      bool isCurrent = (i == m_MessageListCurrentIndex[m_CurrentFolder]);

      // Check if message is selected
      auto selectedUidsIt = m_SelectedUids.find(folder);
      std::set<uint32_t> noSelection;
      const std::set<uint32_t>& folderSelectedUids =
        (selectedUidsIt != m_SelectedUids.end()) ? selectedUidsIt->second : noSelection;
      bool isSelected = (folderSelectedUids.find(uid) != folderSelectedUids.end());

      if (isCurrent)
      {
        wattron(m_MainWin, m_AttrsHighlightedText);
      }

      if (isSelected)
      {
        wattron(m_MainWin, isCurrent ? m_AttrsSelectedHighlighted : m_AttrsSelectedItem);
      }

      std::wstring wheader = Util::TrimPadWString(Util::ToWString(header), m_ScreenWidth - dateWidth - 2) + L" ";
      mvwaddnwstr(m_MainWin, i - idxOffs, 0, wheader.c_str(), std::min((int)wheader.size(), m_ScreenWidth - dateWidth - 2));
      
      // Draw date on the right
      mvwaddstr(m_MainWin, i - idxOffs, m_ScreenWidth - dateWidth - 1, shortDate.c_str());

      if (isSelected)
      {
        wattroff(m_MainWin, isCurrent ? m_AttrsSelectedHighlighted : m_AttrsSelectedItem);
      }

      if (isCurrent)
      {
        wattroff(m_MainWin, m_AttrsHighlightedText);
      }

      const std::map<uint32_t, Body>& bodys = m_Bodys[folder];
      std::set<uint32_t>& requestedBodys = m_RequestedBodys[folder];
      if (i == m_MessageListCurrentIndex[m_CurrentFolder])
      {
        std::lock_guard<std::mutex> lock(m_Mutex);

        if ((bodys.find(uid) == bodys.end()) &&
            (requestedBodys.find(uid) == requestedBodys.end()))
        {
          if (m_PrefetchLevel >= PrefetchLevelCurrentMessage)
          {
            requestedBodys.insert(uid);
            fetchBodyPriUids[folder].insert(uid);
          }
        }

        const std::map<uint32_t, Header>& gheaders = m_Headers[folder];
        std::set<uint32_t>& requestedHeaders = m_RequestedHeaders[folder];
        if ((gheaders.find(uid) == gheaders.end()) &&
            (requestedHeaders.find(uid) == requestedHeaders.end()))
        {
          if (m_PrefetchLevel >= PrefetchLevelCurrentMessage)
          {
            requestedHeaders.insert(uid);
            fetchHeaderUids[folder].insert(uid);
          }
        }
      }
      else if (abs(i - m_MessageListCurrentIndex[m_CurrentFolder]) == 1)
      {
        std::lock_guard<std::mutex> lock(m_Mutex);

        if ((bodys.find(uid) == bodys.end()) &&
            (requestedBodys.find(uid) == requestedBodys.end()))
        {
          if (m_PrefetchLevel >= PrefetchLevelCurrentView)
          {
            requestedBodys.insert(uid);
            fetchBodySecUids[folder].insert(uid);
          }
        }
      }
    }
  }

  for (auto& fetchFlagUid : fetchFlagUids)
  {
    ImapManager::Request request;
    request.m_Folder = fetchFlagUid.first;
    request.m_GetFlags = fetchFlagUid.second;

    LOG_DEBUG_VAR("async req flags =", request.m_GetFlags);
    m_ImapManager->AsyncRequest(request);
  }

  for (auto& fetchHeaderUid : fetchHeaderUids)
  {
    ImapManager::Request request;
    request.m_Folder = fetchHeaderUid.first;
    request.m_GetHeaders = fetchHeaderUid.second;

    LOG_DEBUG_VAR("async req headers =", request.m_GetHeaders);
    m_ImapManager->AsyncRequest(request);
  }

  for (auto& fetchBodyUid : fetchBodyPriUids)
  {
    ImapManager::Request request;
    request.m_Folder = fetchBodyUid.first;
    request.m_GetBodys = fetchBodyUid.second;
    request.m_ProcessHtml = !m_Plaintext;

    LOG_DEBUG_VAR("async req pri bodys =", request.m_GetBodys);
    m_ImapManager->AsyncRequest(request);
  }

  for (auto& fetchBodyUid : fetchBodySecUids)
  {
    ImapManager::Request request;
    request.m_Folder = fetchBodyUid.first;
    request.m_GetBodys = fetchBodyUid.second;
    request.m_ProcessHtml = !m_Plaintext;

    LOG_DEBUG_VAR("async req sec bodys =", request.m_GetBodys);
    m_ImapManager->AsyncRequest(request);
  }

  wrefresh(m_MainWin);
}

void Ui::DrawMessage()
{
  werase(m_MainWin);
  
  // Set background color for the entire message area
  if (m_ColorsEnabled)
  {
    wbkgd(m_MainWin, m_BeautifulColors[COLOR_MESSAGE_BACKGROUND]);
  }

  const std::string& folder = m_CurrentFolderUid.first;
  const int uid = m_CurrentFolderUid.second;

  std::set<uint32_t> fetchHeaderUids;
  std::set<uint32_t> fetchBodyPriUids;
  std::set<uint32_t> fetchBodySecUids;
  bool markSeen = false;
  bool unseen = false;
  {
    std::lock_guard<std::mutex> lock(m_Mutex);

    std::map<uint32_t, Header>& headers = m_Headers[folder];
    std::set<uint32_t>& requestedHeaders = m_RequestedHeaders[folder];

    if ((uid != -1) &&
        (headers.find(uid) == headers.end()) &&
        (requestedHeaders.find(uid) == requestedHeaders.end()))
    {
      requestedHeaders.insert(uid);
      fetchHeaderUids.insert(uid);
    }

    std::map<uint32_t, Body>& bodys = m_Bodys[folder];
    std::set<uint32_t>& requestedBodys = m_RequestedBodys[folder];

    if ((uid != -1) &&
        (bodys.find(uid) == bodys.end()) &&
        (requestedBodys.find(uid) == requestedBodys.end()))
    {
      requestedBodys.insert(uid);
      fetchBodyPriUids.insert(uid);
    }

    std::map<uint32_t, uint32_t>& flags = m_Flags[folder];
    if ((flags.find(uid) != flags.end()) && (!Flag::GetSeen(flags.at(uid))))
    {
      unseen = true;
    }

    std::string headerText;
    std::map<uint32_t, Header>::iterator headerIt = headers.find(uid);
    std::map<uint32_t, Body>::iterator bodyIt = bodys.find(uid);

    std::stringstream ss;
    if (headerIt != headers.end())
    {
      Header& header = headerIt->second;
      if (m_ShowFullHeader)
      {
        ss << header.GetRawHeaderText(m_FullHeaderIncludeLocal);
      }
      else
      {
        ss << "Date: " << header.GetDateTime() << "\n";
        ss << "From: " << header.GetFrom() << "\n";
        if (!header.GetReplyTo().empty())
        {
          ss << "Reply-To: " << header.GetReplyTo() << "\n";
        }

        ss << "To: " << header.GetTo() << "\n";
        if (!header.GetCc().empty())
        {
          ss << "Cc: " << header.GetCc() << "\n";
        }

        if (!header.GetBcc().empty())
        {
          ss << "Bcc: " << header.GetBcc() << "\n";
        }

        ss << "Subject: " << header.GetSubject() << "\n";
      }

      if (bodyIt != bodys.end())
      {
        Body& body = bodyIt->second;
        std::map<ssize_t, PartInfo> parts = body.GetPartInfos();
        std::vector<std::string> attnames;
        for (auto it = parts.begin(); it != parts.end(); ++it)
        {
          if (!it->second.m_Filename.empty())
          {
            attnames.push_back(it->second.m_Filename);
          }
        }

        if (!attnames.empty())
        {
          ss << "Attachments: ";
          ss << Util::Join(attnames, ", ");
          ss << "\n";
        }
      }

      ss << "\n";
    }

    headerText = ss.str();

    if (bodyIt != bodys.end())
    {
      Body& body = bodyIt->second;
      const std::string& bodyText = GetBodyText(body);
      const std::string text = headerText + bodyText;
      m_CurrentMessageViewText = text;
      m_CurrentMessageProcessFlowed = m_RespectFormatFlowed && m_Plaintext && body.IsFormatFlowed();
      std::vector<std::wstring> wlines = GetCachedWordWrapLines(folder, bodyIt->first);
      int countLines = wlines.size();

      m_MessageViewLineOffset = Util::Bound(0, m_MessageViewLineOffset,
                                            countLines - m_MainWinHeight);
      
      // Beautiful message rendering with enhanced colors and formatting
      for (int i = 0; ((i < m_MainWinHeight) && (i < countLines)); ++i)
      {
        const std::wstring& wdispStr = wlines.at(i + m_MessageViewLineOffset);
        const std::string& dispStr = Util::ToString(wdispStr);
        const int lineIndex = i + m_MessageViewLineOffset;
        
        // Determine line type for beautiful coloring
        const bool isHeader = (lineIndex <= m_MessageViewHeaderLineCount);
        const bool isQuote = (dispStr.rfind(">", 0) == 0) && !isHeader;
        const bool isURL = (dispStr.find("http://") != std::string::npos || 
                           dispStr.find("https://") != std::string::npos ||
                           dispStr.find("www.") != std::string::npos ||
                           dispStr.find("@") != std::string::npos);

        // Apply beautiful colors based on content type
        if (isHeader)
        {
          if (dispStr.find("From:") == 0 || dispStr.find("To:") == 0 || 
              dispStr.find("Cc:") == 0 || dispStr.find("Bcc:") == 0 ||
              dispStr.find("Reply-To:") == 0)
          {
            wattron(m_MainWin, m_BeautifulColors[COLOR_HEADER_NAME]);
          }
          else if (dispStr.find("Subject:") == 0)
          {
            wattron(m_MainWin, m_BeautifulColors[COLOR_SUBJECT_TEXT]);
          }
          else if (dispStr.find("Date:") == 0)
          {
            wattron(m_MainWin, m_BeautifulColors[COLOR_DATE_TIME]);
          }
          else if (dispStr.find("Attachments:") == 0)
          {
            wattron(m_MainWin, m_BeautifulColors[COLOR_ATTACHMENT_INFO]);
          }
          else
          {
            wattron(m_MainWin, m_BeautifulColors[COLOR_HEADER_VALUE]);
          }
        }
        else if (isQuote)
        {
          wattron(m_MainWin, m_BeautifulColors[COLOR_QUOTED_TEXT]);
        }
        else if (isURL)
        {
          wattron(m_MainWin, m_BeautifulColors[COLOR_URL_LINK]);
        }
        else
        {
          wattron(m_MainWin, m_BeautifulColors[COLOR_MESSAGE_TEXT]);
        }

        // Handle search highlighting with beautiful colors
        if (!m_MessageFindQuery.empty() && (m_MessageFindMatchLine == lineIndex))
        {
          const std::wstring wquery = Util::ToWString(m_MessageFindQuery);
          std::wstring beforeMatch = wdispStr.substr(0, m_MessageFindMatchPos);
          std::wstring match = wdispStr.substr(m_MessageFindMatchPos, wquery.size());
          std::wstring afterMatch = wdispStr.substr(m_MessageFindMatchPos + wquery.size());

          mvwaddnwstr(m_MainWin, i, 0, beforeMatch.c_str(), beforeMatch.size());
          
          // Beautiful search highlight
          wattron(m_MainWin, m_BeautifulColors[COLOR_SEARCH_MATCH]);
          mvwaddnwstr(m_MainWin, i, beforeMatch.size(), match.c_str(), match.size());
          wattroff(m_MainWin, m_BeautifulColors[COLOR_SEARCH_MATCH]);
          
          mvwaddnwstr(m_MainWin, i, beforeMatch.size() + match.size(), afterMatch.c_str(), afterMatch.size());
        }
        else
        {
          // Normal beautiful rendering
          mvwaddnwstr(m_MainWin, i, 0, wdispStr.c_str(), wdispStr.size());
        }

        // Turn off colors
        if (isHeader)
        {
          if (dispStr.find("From:") == 0 || dispStr.find("To:") == 0 || 
              dispStr.find("Cc:") == 0 || dispStr.find("Bcc:") == 0 ||
              dispStr.find("Reply-To:") == 0)
          {
            wattroff(m_MainWin, m_BeautifulColors[COLOR_HEADER_NAME]);
          }
          else if (dispStr.find("Subject:") == 0)
          {
            wattroff(m_MainWin, m_BeautifulColors[COLOR_SUBJECT_TEXT]);
          }
          else if (dispStr.find("Date:") == 0)
          {
            wattroff(m_MainWin, m_BeautifulColors[COLOR_DATE_TIME]);
          }
          else if (dispStr.find("Attachments:") == 0)
          {
            wattroff(m_MainWin, m_BeautifulColors[COLOR_ATTACHMENT_INFO]);
          }
          else
          {
            wattroff(m_MainWin, m_BeautifulColors[COLOR_HEADER_VALUE]);
          }
        }
        else if (isQuote)
        {
          wattroff(m_MainWin, m_BeautifulColors[COLOR_QUOTED_TEXT]);
        }
        else if (isURL)
        {
          wattroff(m_MainWin, m_BeautifulColors[COLOR_URL_LINK]);
        }
        else
        {
          wattroff(m_MainWin, m_BeautifulColors[COLOR_MESSAGE_TEXT]);
        }
      }

      markSeen = true;
    }

    if (m_PrefetchLevel >= PrefetchLevelCurrentView)
    {
      const std::map<std::string, uint32_t>& displayUids = GetDisplayUids(m_CurrentFolder);
      if (displayUids.size() > 0)
      {
        int32_t maxIndex = (int)GetDisplayUids(m_CurrentFolder).size() - 1;
        int32_t nextIndex = Util::Bound(0, m_MessageListCurrentIndex[m_CurrentFolder] + 1, maxIndex);
        int32_t prevIndex = Util::Bound(0, m_MessageListCurrentIndex[m_CurrentFolder] - 1, maxIndex);
        uint32_t nextUid = std::prev(displayUids.end(), nextIndex + 1)->second;
        uint32_t prevUid = std::prev(displayUids.end(), prevIndex + 1)->second;

        if ((bodys.find(nextUid) == bodys.end()) &&
            (requestedBodys.find(nextUid) == requestedBodys.end()))
        {
          requestedBodys.insert(nextUid);
          fetchBodySecUids.insert(nextUid);
        }

        if ((bodys.find(prevUid) == bodys.end()) &&
            (requestedBodys.find(prevUid) == requestedBodys.end()))
        {
          requestedBodys.insert(prevUid);
          fetchBodySecUids.insert(prevUid);
        }
      }
    }
  }

  if (!fetchHeaderUids.empty())
  {
    ImapManager::Request request;
    request.m_Folder = folder;
    request.m_GetHeaders = fetchHeaderUids;
    LOG_DEBUG_VAR("async req headers =", fetchHeaderUids);
    m_ImapManager->AsyncRequest(request);
  }

  if (!fetchBodyPriUids.empty())
  {
    ImapManager::Request request;
    request.m_Folder = folder;
    request.m_GetBodys = fetchBodyPriUids;
    request.m_ProcessHtml = !m_Plaintext;
    LOG_DEBUG_VAR("async req pri bodys =", fetchBodyPriUids);
    m_ImapManager->AsyncRequest(request);
  }

  if (!fetchBodySecUids.empty())
  {
    ImapManager::Request request;
    request.m_Folder = folder;
    request.m_GetBodys = fetchBodySecUids;
    request.m_ProcessHtml = !m_Plaintext;
    LOG_DEBUG_VAR("async req sec bodys =", fetchBodySecUids);
    m_ImapManager->AsyncRequest(request);
  }

  if (unseen && markSeen && !m_MessageViewToggledSeen)
  {
    MarkSeen();
  }

  // Beautiful status line for message view
  std::string messageInfo = "Message View - Use arrow keys to scroll";
  DrawBeautifulStatusLine(messageInfo, "message");

  wrefresh(m_MainWin);
}

void Ui::DrawComposeMessage()
{
  const bool processFlowed = false; // only process when viewing message
  const bool outputFlowed = false; // only generate when sending after compose
  const bool quoteWrap = false; // only wrap quoted lines when viewing message
  const int expandTabSize = 0; // disabled
  m_ComposeMessageLines = Util::WordWrap(m_ComposeMessageStr, m_MaxComposeLineLength,
                                         processFlowed, outputFlowed, quoteWrap, expandTabSize,
                                         m_ComposeMessagePos, m_ComposeMessageWrapLine,
                                         m_ComposeMessageWrapPos);

  std::vector<std::wstring> headerLines;
  if (m_ShowRichHeader)
  {
    headerLines =
    {
      L"From    : ",
      L"To      : ",
      L"Cc      : ",
      L"Bcc     : ",
      L"Attchmnt: ",
      L"Subject : ",
    };
  }
  else
  {
    headerLines =
    {
      L"To      : ",
      L"Cc      : ",
      L"Attchmnt: ",
      L"Subject : ",
    };
  }

  int cursY = 0;
  int cursX = 0;
  if (m_IsComposeHeader)
  {
    m_ComposeHeaderLine = std::min(m_ComposeHeaderLine, (int)m_ComposeHeaderStr.size() - 1);
    cursY = m_ComposeHeaderLine;
    cursX = m_ComposeHeaderPos + 10;
  }
  else
  {
    cursY = headerLines.size() + 1 + m_ComposeMessageWrapLine;
    cursX = m_ComposeMessageWrapPos;
  }

  werase(m_MainWin);
  
  // Set background color for the entire compose area
  if (m_ColorsEnabled)
  {
    wbkgd(m_MainWin, m_BeautifulColors[COLOR_MESSAGE_BACKGROUND]);
  }

  std::vector<std::wstring> composeLines;

  for (int i = 0; i < (int)headerLines.size(); ++i)
  {
    if (m_IsComposeHeader && (i == m_ComposeHeaderLine) && (cursX >= m_ScreenWidth))
    {
      std::wstring line = headerLines.at(i) +
        m_ComposeHeaderStr.at(i).substr(cursX - m_ScreenWidth + 1);
      composeLines.push_back(line.substr(0, m_ScreenWidth));
      cursX = m_ScreenWidth - 1;
    }
    else
    {
      std::wstring line = headerLines.at(i) + m_ComposeHeaderStr.at(i);
      composeLines.push_back(line.substr(0, m_ScreenWidth));
    }
  }

  composeLines.push_back(L"");

  for (auto line = m_ComposeMessageLines.begin(); line != m_ComposeMessageLines.end(); ++line)
  {
    composeLines.push_back(*line);
  }

  if (cursY < m_ComposeMessageOffsetY)
  {
    m_ComposeMessageOffsetY = std::max(m_ComposeMessageOffsetY - (m_MainWinHeight / 2), 0);

  }
  else if (cursY >= (m_ComposeMessageOffsetY + m_MainWinHeight))
  {
    m_ComposeMessageOffsetY += (m_MainWinHeight / 2);
  }

  int messageY = 0;
  int idx = 0;
  for (auto line = composeLines.begin(); line != composeLines.end(); ++line, ++idx)
  {
    if (idx < m_ComposeMessageOffsetY) continue;

    if (messageY > m_MainWinHeight) break;

    const std::string& dispStr = Util::ToString(*line);
    const bool isQuote = (dispStr.rfind(">", 0) == 0);

    if (isQuote)
    {
      wattron(m_MainWin, m_AttrsQuotedText);
    }

    mvwprintw(m_MainWin, messageY, 0, "%s", dispStr.c_str());

    if (isQuote)
    {
      wattroff(m_MainWin, m_AttrsQuotedText);
    }

    ++messageY;
  }

  cursY -= m_ComposeMessageOffsetY;

  leaveok(m_MainWin, false);
  wmove(m_MainWin, cursY, cursX);
  wrefresh(m_MainWin);
  leaveok(m_MainWin, true);
}

void Ui::DrawPartList()
{
  werase(m_MainWin);
  
  // Set background color for the entire part list area
  if (m_ColorsEnabled)
  {
    wbkgd(m_MainWin, m_BeautifulColors[COLOR_MESSAGE_BACKGROUND]);
  }

  std::lock_guard<std::mutex> lock(m_Mutex);
  const std::string& folder = m_CurrentFolderUid.first;
  const int uid = m_CurrentFolderUid.second;
  std::map<uint32_t, Body>& bodys = m_Bodys[folder];
  std::map<uint32_t, Body>::iterator bodyIt = bodys.find(uid);
  if (bodyIt != bodys.end())
  {
    Body& body = bodyIt->second;
    const std::map<ssize_t, PartInfo>& parts = body.GetPartInfos();

    int count = parts.size();
    if (count > 0)
    {
      m_PartListCurrentIndex = Util::Bound(0, m_PartListCurrentIndex, (int)parts.size() - 1);

      int itemsMax = m_MainWinHeight - 1;
      int idxOffs = Util::Bound(0, (int)(m_PartListCurrentIndex - ((itemsMax - 1) / 2)),
                                std::max(0, (int)parts.size() - (int)itemsMax));
      int idxMax = idxOffs + std::min(itemsMax, (int)parts.size());

      for (int i = idxOffs; i < idxMax; ++i)
      {
        auto it = std::next(parts.begin(), i);
        const PartInfo& part = it->second;

        if (i == m_PartListCurrentIndex)
        {
          wattron(m_MainWin, m_AttrsHighlightedText);
          m_PartListCurrentPartInfo = part;
        }

        std::string leftPad = "    ";
        std::string sizeStr = std::to_string(part.m_Size) + " bytes";
        std::string sizeStrPadded = Util::TrimPadString(sizeStr, 17) + " ";
        std::string mimeTypePadded = Util::TrimPadString(part.m_MimeType, 29) + " ";
        std::wstring wline = Util::ToWString(leftPad + sizeStrPadded + mimeTypePadded);
        std::wstring wfilename = Util::ToWString(part.m_Filename);
        int filenameMaxLen = m_ScreenWidth - (int)wline.size();
        std::wstring wfilenamePadded = Util::TrimPadWString(wfilename, filenameMaxLen - 1) + L" ";
        wline = wline + wfilenamePadded;

        mvwaddnwstr(m_MainWin, i - idxOffs, 0, wline.c_str(),
                    std::min((int)wline.size(), m_ScreenWidth));

        if (i == m_PartListCurrentIndex)
        {
          wattroff(m_MainWin, m_AttrsHighlightedText);
        }
      }
    }
  }

  wrefresh(m_MainWin);
}

void Ui::AsyncUiRequest(char p_UiRequest)
{
  LOG_IF_NOT_EQUAL(write(m_Pipe[1], &p_UiRequest, 1), 1);
}

void Ui::PerformUiRequest(char p_UiRequest)
{
  if (p_UiRequest & UiRequestDrawAll)
  {
    DrawAll();
  }

  if (p_UiRequest & UiRequestDrawError)
  {
    std::unique_lock<std::mutex> lock(m_SmtpErrorMutex);
    if (!m_SmtpErrorResults.empty())
    {
      const SmtpManager::Result result = m_SmtpErrorResults.front();
      m_SmtpErrorResults.pop_front();
      lock.unlock();

      SmtpResultHandlerError(result);
    }
  }

  if (p_UiRequest & UiRequestHandleConnected)
  {
    HandleConnected();
  }
}

void Ui::Run()
{
  DrawAll();
  int64_t uiIdleTime = 0;
  LOG_INFO("entering ui loop");
  Util::InitUiSignalHandlers();
  raw();

  while (s_Running)
  {
    fd_set fds;
    FD_ZERO(&fds);
    FD_SET(STDIN_FILENO, &fds);
    FD_SET(m_Pipe[0], &fds);
    int maxfd = std::max(STDIN_FILENO, m_Pipe[0]);
    struct timeval tv = {1, 0}; // uiIdleTime logic below is dependent on timeout value
    int rv = select(maxfd + 1, &fds, NULL, NULL, &tv);

    if (rv == 0)
    {
      if (++uiIdleTime >= 600) // ui idle refresh every 10 minutes
      {
        PerformUiRequest(UiRequestDrawAll);
        uiIdleTime = 0;
      }

      continue;
    }

    uiIdleTime = 0;

    if (FD_ISSET(STDIN_FILENO, &fds))
    {
      wint_t key = 0;
      get_wch(&key);

      if (key == KEY_RESIZE)
      {
        CleanupWindows();
        InitWindows();
        DrawAll();
        continue;
      }

      switch (m_State)
      {
        case StateViewMessageList:
          ViewMessageListKeyHandler(key);
          break;

        case StateViewMessage:
          ViewMessageKeyHandler(key);
          break;

        case StateGotoFolder:
        case StateMoveToFolder:
          ViewFolderListKeyHandler(key);
          break;

        case StateComposeMessage:
        case StateComposeCopyMessage:
        case StateReplyAllMessage:
        case StateReplySenderMessage:
        case StateForwardMessage:
        case StateForwardAttachedMessage:
          ComposeMessageKeyHandler(key);
          break;

        case StateAddressList:
        case StateFromAddressList:
          ViewAddressListKeyHandler(key);
          break;

        case StateFileList:
          ViewFileListKeyHandler(key);
          break;

        case StateViewPartList:
          ViewPartListKeyHandler(key);
          break;

        default:
          break;
      }

      continue;
    }

    if (FD_ISSET(m_Pipe[0], &fds))
    {
      int len = 0;
      ioctl(m_Pipe[0], FIONREAD, &len);
      if (len > 0)
      {
        len = std::min(len, 256);
        std::vector<char> buf(len);
        LOG_IF_NOT_EQUAL(read(m_Pipe[0], &buf[0], len), len);
        char uiRequest = UiRequestNone;
        for (int i = 0; i < len; ++i)
        {
          uiRequest |= buf[i];
        }

        PerformUiRequest(uiRequest);
      }
    }

  }

  noraw();
  Util::CleanupUiSignalHandlers();
  LOG_INFO("exiting ui loop");

  return;
}

void Ui::ViewFolderListKeyHandler(int p_Key)
{
  if (p_Key == m_KeyCancel)
  {
    SetState(StateViewMessageList);
  }
  else if ((p_Key == KEY_RETURN) || (p_Key == KEY_ENTER) ||
           ((p_Key == KEY_RIGHT) && (m_FolderListFilterPos == (int)m_FolderListFilterStr.size())))
  {
    if (m_FolderListCurrentFolder != "")
    {
      if (m_State == StateGotoFolder)
      {
        m_CurrentFolder = m_FolderListCurrentFolder;
        m_ImapManager->SetCurrentFolder(m_CurrentFolder);
        SetState(StateViewMessageList);
        UpdateIndexFromUid();
      }
      else if (m_State == StateMoveToFolder)
      {
        const std::string& folder = m_CurrentFolderUid.first;
        if (m_FolderListCurrentFolder != folder)
        {
          MoveSelectedMessages(m_FolderListCurrentFolder);
          SetLastStateOrMessageList();
        }
        else
        {
          SetDialogMessage("Move to same folder ignored");
          UpdateUidFromIndex(true /* p_UserTriggered */);
          SetState(m_LastState);
        }

        if (m_PersistFolderFilter)
        {
          m_PersistedFolderListFilterPos = m_FolderListFilterPos;
          m_PersistedFolderListFilterStr = m_FolderListFilterStr;
          m_PersistedFolderListCurrentFolder = m_FolderListCurrentFolder;
          m_PersistedFolderListCurrentIndex = m_FolderListCurrentIndex;
        }
      }

      ClearSelection();
    }
  }
  else if (HandleListKey(p_Key, m_FolderListCurrentIndex))
  {
    // none
  }
  else if (HandleLineKey(p_Key, m_FolderListFilterStr, m_FolderListFilterPos))
  {
    // none
  }
  else if (HandleTextKey(p_Key, m_FolderListFilterStr, m_FolderListFilterPos))
  {
    // none
  }

  DrawAll();
}

void Ui::ViewAddressListKeyHandler(int p_Key)
{
  if (p_Key == m_KeyCancel)
  {
    SetState(m_LastMessageState);
  }
  else if ((p_Key == KEY_RETURN) || (p_Key == KEY_ENTER) ||
           ((p_Key == KEY_RIGHT) && (m_AddressListFilterPos == (int)m_AddressListFilterStr.size())))
  {
    if (m_State == StateAddressList)
    {
      AddAddress(m_AddressListCurrentAddress);
    }
    else if (m_State == StateFromAddressList)
    {
      SetAddress(m_AddressListCurrentAddress);
    }
    SetState(m_LastMessageState);
  }
  else if (HandleListKey(p_Key, m_AddressListCurrentIndex))
  {
    // none
  }
  else if (HandleLineKey(p_Key, m_AddressListFilterStr, m_AddressListFilterPos))
  {
    // none
  }
  else if (HandleTextKey(p_Key, m_AddressListFilterStr, m_AddressListFilterPos))
  {
    // none
  }

  DrawAll();
}

void Ui::ViewFileListKeyHandler(int p_Key)
{
  if (p_Key == m_KeyCancel)
  {
    SetState(m_LastMessageState);
  }
  else if ((p_Key == KEY_RETURN) || (p_Key == KEY_ENTER) ||
           ((p_Key == KEY_RIGHT) && (m_FileListFilterPos == (int)m_FileListFilterStr.size())))
  {
    if (m_FileListCurrentFile.IsDir())
    {
      m_FileListFilterPos = 0;
      m_FileListFilterStr.clear();
      m_CurrentDir = Util::AbsolutePath(m_CurrentDir + "/" + m_FileListCurrentFile.m_Name);
      m_Files = Util::ListPaths(m_CurrentDir);
      m_FileListCurrentIndex = 0;
      m_FileListCurrentFile.m_Name = "";
    }
    else
    {
      std::string newFilePath = Util::AbsolutePath(m_CurrentDir + "/" + m_FileListCurrentFile.m_Name);
      AddAttachmentPath(newFilePath);
      SetState(m_LastMessageState);
    }
  }
  else if ((m_FileListFilterPos == 0) && (p_Key == KEY_LEFT))
  {
    m_FileListFilterPos = 0;
    m_FileListFilterStr.clear();
    std::string lastDirName = Util::BaseName(m_CurrentDir);
    m_CurrentDir = Util::AbsolutePath(m_CurrentDir + "/..");
    m_Files = Util::ListPaths(m_CurrentDir);
    if (lastDirName != "/")
    {
      Fileinfo lastDirFileinfo(lastDirName, -1);
      auto it = m_Files.find(lastDirFileinfo);
      if (it != m_Files.end())
      {
        m_FileListCurrentIndex = std::distance(m_Files.begin(), it);
        m_FileListCurrentFile = *it;
      }
    }
    else
    {
      m_FileListCurrentIndex = 0;
      m_FileListCurrentFile.m_Name = "";
    }
  }
  else if ((m_FileListFilterPos == 0) && ((p_Key == KEY_BACKSPACE) || (p_Key == KEY_DELETE)))
  {
    // backspace with empty filter goes up one directory level
    m_FileListFilterPos = 0;
    m_FileListFilterStr.clear();
    m_CurrentDir = Util::AbsolutePath(m_CurrentDir + "/..");
    m_Files = Util::ListPaths(m_CurrentDir);
    m_FileListCurrentIndex = 0;
    m_FileListCurrentFile.m_Name = "";
  }
  else if (HandleListKey(p_Key, m_FileListCurrentIndex))
  {
    // none
  }
  else if (HandleLineKey(p_Key, m_FileListFilterStr, m_FileListFilterPos))
  {
    // none
  }
  else if (HandleTextKey(p_Key, m_FileListFilterStr, m_FileListFilterPos))
  {
    // none
  }

  DrawAll();
}

void Ui::ViewMessageListKeyHandler(int p_Key)
{
  if (p_Key == m_KeyQuit)
  {
    Quit();
  }
  else if (p_Key == m_KeyRefresh)
  {
    if (IsConnected())
    {
      InvalidateUiCache(m_CurrentFolder);
    }
    else
    {
      SetDialogMessage("Cannot refresh while offline");
    }
  }
  else if ((p_Key == KEY_UP) || (p_Key == m_KeyPrevMsg))
  {
    --m_MessageListCurrentIndex[m_CurrentFolder];
    UpdateUidFromIndex(true /* p_UserTriggered */);
  }
  else if ((p_Key == KEY_DOWN) || (p_Key == m_KeyNextMsg))
  {
    ++m_MessageListCurrentIndex[m_CurrentFolder];
    UpdateUidFromIndex(true /* p_UserTriggered */);
  }
  else if (p_Key == m_KeyPrevPage)
  {
    m_MessageListCurrentIndex[m_CurrentFolder] =
      m_MessageListCurrentIndex[m_CurrentFolder] - m_MainWinHeight;
    UpdateUidFromIndex(true /* p_UserTriggered */);
  }
  else if (p_Key == m_KeyNextPage)
  {
    m_MessageListCurrentIndex[m_CurrentFolder] =
      m_MessageListCurrentIndex[m_CurrentFolder] + m_MainWinHeight;
    UpdateUidFromIndex(true /* p_UserTriggered */);
  }
  else if (p_Key == KEY_HOME)
  {
    m_MessageListCurrentIndex[m_CurrentFolder] = 0;
    UpdateUidFromIndex(true /* p_UserTriggered */);
  }
  else if (p_Key == KEY_END)
  {
    m_MessageListCurrentIndex[m_CurrentFolder] = std::numeric_limits<int>::max();
    UpdateUidFromIndex(true /* p_UserTriggered */);
  }
  else if ((p_Key == KEY_RETURN) || (p_Key == KEY_ENTER) || (p_Key == m_KeyOpen) || (p_Key == KEY_RIGHT))
  {
    UpdateUidFromIndex(true /* p_UserTriggered */);
    const int uid = m_CurrentFolderUid.second;
    if (uid != -1)
    {
      if (m_MessageListSearch)
      {
        m_MessageFindMatchLine = -1;
        m_MessageFindQuery = m_MessageListSearchQuery;

        // remove chars not supported by find in document
        Util::RemoveChar(m_MessageFindQuery, '"');
        Util::RemoveChar(m_MessageFindQuery, '*');
      }

      SetState(StateViewMessage);
    }
  }
  else if ((p_Key == m_KeyGotoFolder) || (p_Key == m_KeyBack) || (p_Key == KEY_LEFT))
  {
    if (m_MessageListSearch)
    {
      m_MessageListSearch = false;
      m_CurrentFolder = m_PreviousFolder;
      m_PreviousFolder = "";
      UpdateIndexFromUid();
    }
    else
    {
      if (!m_PersistSortFilter)
      {
        DisableSortFilter();
      }

      SetState(StateGotoFolder);
    }
  }
  else if (p_Key == m_KeyGotoInbox)
  {
    if (m_MessageListSearch)
    {
      m_MessageListSearch = false;
      m_PreviousFolder = "";
    }

    if (m_CurrentFolder != m_Inbox)
    {
      m_CurrentFolder = m_Inbox;
    }
    else
    {
      m_MessageListCurrentIndex[m_CurrentFolder] = 0;
      UpdateUidFromIndex(true /* p_UserTriggered */);
    }

    SetState(StateViewMessageList);
  }
  else if ((p_Key == m_KeyMove) || (p_Key == m_KeyAutoMove))
  {
    if (IsConnected())
    {
      UpdateUidFromIndex(true /* p_UserTriggered */);
      const int uid = m_CurrentFolderUid.second;
      if (uid != -1)
      {
        m_IsAutoMove = (p_Key != m_KeyMove);
        SetState(StateMoveToFolder);
      }
      else
      {
        SetDialogMessage("No message to move");
      }
    }
    else
    {
      SetDialogMessage("Cannot move while offline");
    }
  }
  else if (p_Key == m_KeyCompose)
  {
    SetState(StateComposeMessage);
  }
  else if (p_Key == m_KeyComposeCopy)
  {
    UpdateUidFromIndex(true /* p_UserTriggered */);
    const int uid = m_CurrentFolderUid.second;
    if (uid != -1)
    {
      if (CurrentMessageBodyHeaderAvailable())
      {
        SetState(StateComposeCopyMessage);
      }
      else
      {
        SetDialogMessage("Cannot compose copy of message not fetched");
      }
    }
    else
    {
      SetDialogMessage("No message to copy for compose");
    }
  }
  else if ((p_Key == m_KeyReplyAll) || (p_Key == m_KeyReplySender))
  {
    UpdateUidFromIndex(true /* p_UserTriggered */);
    const int uid = m_CurrentFolderUid.second;
    if (uid != -1)
    {
      if (CurrentMessageBodyHeaderAvailable())
      {
        SetState((p_Key == m_KeyReplyAll) ? StateReplyAllMessage : StateReplySenderMessage);
      }
      else
      {
        SetDialogMessage("Cannot reply message not fetched");
      }
    }
    else
    {
      SetDialogMessage("No message to reply");
    }
  }
  else if (p_Key == m_KeyForward)
  {
    UpdateUidFromIndex(true /* p_UserTriggered */);
    const int uid = m_CurrentFolderUid.second;
    if (uid != -1)
    {
      if (CurrentMessageBodyHeaderAvailable())
      {
        SetState(StateForwardMessage);
      }
      else
      {
        SetDialogMessage("Cannot forward message not fetched");
      }
    }
    else
    {
      SetDialogMessage("No message to forward");
    }
  }
  else if (p_Key == m_KeyForwardAttached)
  {
    UpdateUidFromIndex(true /* p_UserTriggered */);
    const int uid = m_CurrentFolderUid.second;
    if (uid != -1)
    {
      if (CurrentMessageBodyHeaderAvailable())
      {
        SetState(StateForwardAttachedMessage);
      }
      else
      {
        SetDialogMessage("Cannot forward message not fetched");
      }
    }
    else
    {
      SetDialogMessage("No message to forward");
    }
  }
  else if ((p_Key == m_KeyDelete) || (p_Key == KEY_DC))
  {
    if (IsConnected())
    {
      UpdateUidFromIndex(true /* p_UserTriggered */);
      const int uid = m_CurrentFolderUid.second;
      if (uid != -1)
      {
        DeleteMessage();
      }
      else
      {
        SetDialogMessage("No message to delete");
      }
    }
    else
    {
      SetDialogMessage("Cannot delete while offline");
    }
  }
  else if (p_Key == m_KeyToggleUnread)
  {
    if (IsConnected())
    {
      UpdateUidFromIndex(true /* p_UserTriggered */);
      const int uid = m_CurrentFolderUid.second;
      if (uid != -1)
      {
        ToggleSeen();
      }
      else
      {
        SetDialogMessage("No message to toggle read/unread");
      }
    }
    else
    {
      SetDialogMessage("Cannot toggle read/unread while offline");
    }
  }
  else if (p_Key == m_KeyOtherCmdHelp)
  {
    m_HelpViewMessagesListOffset += 2;
    if (m_HelpViewMessagesListOffset >= m_HelpViewMessagesListSize)
    {
      m_HelpViewMessagesListOffset = 0;
    }
  }
  else if (p_Key == m_KeyExport)
  {
    UpdateUidFromIndex(true /* p_UserTriggered */);
    const int uid = m_CurrentFolderUid.second;
    if (uid != -1)
    {
      ExportMessage();
    }
    else
    {
      SetDialogMessage("No message to export");
    }
  }
  else if (p_Key == m_KeyImport)
  {
    if (IsConnected())
    {
      ImportMessage();
    }
    else
    {
      SetDialogMessage("Cannot import while offline");
    }
  }
  else if (p_Key == m_KeySearch)
  {
    SearchMessage();
  }
  else if (p_Key == m_KeySync)
  {
    StartSync();
  }
  else if (p_Key == m_KeyExtHtmlViewer)
  {
    UpdateUidFromIndex(true /* p_UserTriggered */);
    ExtHtmlViewer();
  }
  else if (p_Key == m_KeyExtMsgViewer)
  {
    UpdateUidFromIndex(true /* p_UserTriggered */);
    ExtMsgViewer();
  }
  else if ((p_Key == m_KeyFilterSortReset) && !m_MessageListSearch)
  {
    DisableSortFilter();
  }
  else if ((p_Key == m_KeyFilterShowUnread) && !m_MessageListSearch)
  {
    ToggleFilter(SortUnseenOnly);
  }
  else if ((p_Key == m_KeyFilterShowHasAttachments) && !m_MessageListSearch)
  {
    ToggleFilter(SortAttchOnly);
  }
  else if ((p_Key == m_KeyFilterShowCurrentDate) && !m_MessageListSearch)
  {
    UpdateUidFromIndex(true /* p_UserTriggered */);
    ToggleFilter(SortCurrDateOnly);
  }
  else if ((p_Key == m_KeyFilterShowCurrentName) && !m_MessageListSearch)
  {
    UpdateUidFromIndex(true /* p_UserTriggered */);
    ToggleFilter(SortCurrNameOnly);
  }
  else if ((p_Key == m_KeyFilterShowCurrentSubject) && !m_MessageListSearch)
  {
    UpdateUidFromIndex(true /* p_UserTriggered */);
    ToggleFilter(SortCurrSubjOnly);
  }
  else if ((p_Key == m_KeySortUnread) && !m_MessageListSearch)
  {
    ToggleSort(SortUnseenDesc, SortUnseenAsc);
  }
  else if ((p_Key == m_KeySortHasAttachments) && !m_MessageListSearch)
  {
    ToggleSort(SortAttchDesc, SortAttchAsc);
  }
  else if ((p_Key == m_KeySortDate) && !m_MessageListSearch)
  {
    ToggleSort(SortDateDesc, SortDateAsc);
  }
  else if ((p_Key == m_KeySortName) && !m_MessageListSearch)
  {
    ToggleSort(SortNameDesc, SortNameAsc);
  }
  else if ((p_Key == m_KeySortSubject) && !m_MessageListSearch)
  {
    ToggleSort(SortSubjDesc, SortSubjAsc);
  }
  else if ((p_Key == m_KeyJumpTo) && m_MessageListSearch)
  {
    if (m_MessageListSearch)
    {
      m_MessageListSearch = false;
      m_CurrentFolder = m_CurrentFolderUid.first;
      const uint32_t uid = m_CurrentFolderUid.second;

      if (!m_HasRequestedUids[m_CurrentFolder])
      {
        ImapManager::Request request;
        request.m_Folder = m_CurrentFolder;
        request.m_GetUids = true;
        request.m_GetHeaders = std::set<uint32_t>({ uid });
        LOG_DEBUG_VAR("async req uids =", m_CurrentFolder);
        m_HasRequestedUids[m_CurrentFolder] = true;
        m_ImapManager->AsyncRequest(request);

        bool found = false;
        int totalWaitMs = 0;
        const int stepSleepMs = 10;
        const int maxWaitMs = 2000; // max wait for fetching folder headers from cache
        while ((totalWaitMs < maxWaitMs) && !found)
        {
          usleep(stepSleepMs * 1000);
          totalWaitMs += stepSleepMs;
          {
            std::lock_guard<std::mutex> lock(m_Mutex);
            std::map<uint32_t, Header>& headers = m_Headers[m_CurrentFolder];
            std::set<uint32_t>& uids = m_Uids[m_CurrentFolder];

            if ((headers.find(uid) != headers.end()) && (uids.size() == headers.size()))
            {
              found = true;
            }
          }
        }
      }
      else
      {
        LOG_DEBUG("jump fetch not needed");
      }

      m_MessageListCurrentUid[m_CurrentFolder] = uid;
      m_MessageListUidSet[m_CurrentFolder] = true;
      UpdateIndexFromUid();
    }
  }
  else if (p_Key == m_KeySelectItem)
  {
    const int uid = m_CurrentFolderUid.second;
    if (uid != -1)
    {
      ToggleSelected();

      ++m_MessageListCurrentIndex[m_CurrentFolder];
      UpdateUidFromIndex(true /* p_UserTriggered */);
    }
    else
    {
      SetDialogMessage("No message to select/unselect");
    }
  }
  else if (p_Key == m_KeySelectAll)
  {
    const int uid = m_CurrentFolderUid.second;
    if (uid != -1)
    {
      ToggleSelectAll();
    }
    else
    {
      SetDialogMessage("No messages to select/unselect");
    }
  }
  else if ((p_Key == m_KeySearchShowFolder) && m_MessageListSearch)
  {
    m_SearchShowFolder = !m_SearchShowFolder;
  }
  else if (p_Key == m_KeySearchCurrentSubject)
  {
    UpdateUidFromIndex(true /* p_UserTriggered */);
    SearchMessageBasedOnCurrent(true /* p_Subject */);
  }
  else if (p_Key == m_KeySearchCurrentName)
  {
    UpdateUidFromIndex(true /* p_UserTriggered */);
    SearchMessageBasedOnCurrent(false /* p_Subject */);
  }
  else if (m_InvalidInputNotify)
  {
    SetDialogMessage("Invalid input (" + Util::ToHexString(p_Key) + ")");
  }

  DrawAll();
}

void Ui::ViewMessageKeyHandler(int p_Key)
{
  if (p_Key == m_KeyQuit)
  {
    Quit();
  }
  else if (p_Key == m_KeyPrevMsg)
  {
    int prevIndex = m_MessageListCurrentIndex[m_CurrentFolder]--;
    UpdateUidFromIndex(true /* p_UserTriggered */);
    if (prevIndex == m_MessageListCurrentIndex[m_CurrentFolder])
    {
      SetDialogMessage("Already on first message");
    }
    else
    {
      m_MessageViewLineOffset = 0;
      m_MessageFindMatchLine = -1;
    }
  }
  else if (p_Key == m_KeyNextMsg)
  {
    int prevIndex = m_MessageListCurrentIndex[m_CurrentFolder]++;
    UpdateUidFromIndex(true /* p_UserTriggered */);
    if (prevIndex == m_MessageListCurrentIndex[m_CurrentFolder])
    {
      SetDialogMessage("No more messages");
    }
    else
    {
      m_MessageViewLineOffset = 0;
      m_MessageFindMatchLine = -1;
    }
  }
  else if (HandleListKey(p_Key, m_MessageViewLineOffset))
  {
    // none
  }
  else if (p_Key == KEY_SPACE) // alternative next page key
  {
    m_MessageViewLineOffset = m_MessageViewLineOffset + m_MainWinHeight;
  }
  else if ((p_Key == KEY_BACKSPACE) || (p_Key == KEY_DELETE) || (p_Key == m_KeyBack) || (p_Key == KEY_LEFT))
  {
    SetState(StateViewMessageList);
  }
  else if ((p_Key == m_KeyOpen) || (p_Key == KEY_RIGHT))
  {
    SetState(StateViewPartList);
  }
  else if (p_Key == m_KeyGotoFolder)
  {
    SetState(StateGotoFolder);
  }
  else if (p_Key == m_KeyGotoInbox)
  {
    if (m_MessageListSearch)
    {
      m_MessageListSearch = false;
      m_PreviousFolder = "";
    }

    m_CurrentFolder = m_Inbox;
    SetState(StateViewMessageList);
  }
  else if ((p_Key == m_KeyMove) || (p_Key == m_KeyAutoMove))
  {
    if (IsConnected())
    {
      ClearSelection();
      m_IsAutoMove = (p_Key != m_KeyMove);
      SetState(StateMoveToFolder);
    }
    else
    {
      SetDialogMessage("Cannot move while offline");
    }
  }
  else if (p_Key == m_KeyCompose)
  {
    SetState(StateComposeMessage);
  }
  else if (p_Key == m_KeyComposeCopy)
  {
    if (CurrentMessageBodyHeaderAvailable())
    {
      SetState(StateComposeCopyMessage);
    }
    else
    {
      SetDialogMessage("Cannot compose copy of message not fetched");
    }
  }
  else if ((p_Key == m_KeyReplyAll) || (p_Key == m_KeyReplySender))
  {
    if (CurrentMessageBodyHeaderAvailable())
    {
      SetState((p_Key == m_KeyReplyAll) ? StateReplyAllMessage : StateReplySenderMessage);
    }
    else
    {
      SetDialogMessage("Cannot reply message not fetched");
    }
  }
  else if (p_Key == m_KeyForward)
  {
    if (CurrentMessageBodyHeaderAvailable())
    {
      SetState(StateForwardMessage);
    }
    else
    {
      SetDialogMessage("Cannot forward message not fetched");
    }
  }
  else if (p_Key == m_KeyForwardAttached)
  {
    if (CurrentMessageBodyHeaderAvailable())
    {
      SetState(StateForwardAttachedMessage);
    }
    else
    {
      SetDialogMessage("Cannot forward message not fetched");
    }
  }
  else if (p_Key == m_KeyToggleTextHtml)
  {
    m_Plaintext = !m_Plaintext;
    m_MessageViewLineOffset = 0;
    m_MessageFindMatchLine = -1;
  }
  else if ((p_Key == m_KeyDelete) || (p_Key == KEY_DC))
  {
    if (IsConnected())
    {
      ClearSelection();
      DeleteMessage();
    }
    else
    {
      SetDialogMessage("Cannot delete while offline");
    }
  }
  else if (p_Key == m_KeyToggleUnread)
  {
    if (IsConnected())
    {
      {
        std::lock_guard<std::mutex> lock(m_Mutex);
        m_MessageViewToggledSeen = true;
      }
      ToggleSeen();
    }
    else
    {
      SetDialogMessage("Cannot toggle read/unread while offline");
    }
  }
  else if (p_Key == m_KeyOtherCmdHelp)
  {
    m_HelpViewMessageOffset = (m_HelpViewMessageOffset == 0) ? 2 : 0;
  }
  else if (p_Key == m_KeyExport)
  {
    ExportMessage();
  }
  else if (p_Key == m_KeyExtPager)
  {
    ExtPager();
  }
  else if (p_Key == m_KeyExtHtmlViewer)
  {
    ExtHtmlViewer();
  }
  else if (p_Key == m_KeyExtMsgViewer)
  {
    ExtMsgViewer();
  }
  else if (p_Key == m_KeyFind)
  {
    MessageFind();
  }
  else if (p_Key == m_KeyFindNext)
  {
    if (!m_MessageFindQuery.empty())
    {
      MessageFindNext();
    }
    else
    {
      SetDialogMessage("Find text not set");
    }
  }
  else if (p_Key == m_KeyToggleFullHeader)
  {
    m_ShowFullHeader = !m_ShowFullHeader;
    m_MessageViewLineOffset = 0;
    m_MessageFindMatchLine = -1;
  }
  else if (m_InvalidInputNotify)
  {
    SetDialogMessage("Invalid input (" + Util::ToHexString(p_Key) + ")");
  }

  DrawAll();
}

void Ui::ComposeMessageKeyHandler(int p_Key)
{
  bool continueProcess = false;
  bool asyncRedraw = false;

  // process state-specific key handling first
  if (m_IsComposeHeader)
  {
    if (p_Key == KEY_UP)
    {
      --m_ComposeHeaderLine;
      if (m_ComposeHeaderLine < 0)
      {
        m_ComposeHeaderPos = 0;
      }

      m_ComposeHeaderLine = Util::Bound(0, m_ComposeHeaderLine,
                                        (int)m_ComposeHeaderStr.size() - 1);
      m_ComposeHeaderPos = Util::Bound(0, m_ComposeHeaderPos,
                                       (int)m_ComposeHeaderStr.at(m_ComposeHeaderLine).size());
    }
    else if ((p_Key == KEY_DOWN) || (p_Key == KEY_RETURN) || (p_Key == KEY_ENTER) || (p_Key == KEY_TAB))
    {
      if (m_ComposeHeaderLine < ((int)m_ComposeHeaderStr.size() - 1))
      {
        m_ComposeHeaderLine = Util::Bound(0, m_ComposeHeaderLine + 1,
                                          (int)m_ComposeHeaderStr.size() - 1);
        m_ComposeHeaderPos = Util::Bound(0, m_ComposeHeaderPos,
                                         (int)m_ComposeHeaderStr.at(m_ComposeHeaderLine).size());
      }
      else
      {
        m_IsComposeHeader = false;
      }
    }
    else if (p_Key == m_KeyPrevPageCompose)
    {
      m_ComposeHeaderLine = 0;
      m_ComposeHeaderPos = 0;
    }
    else if (p_Key == m_KeyNextPageCompose)
    {
      m_IsComposeHeader = false;
    }
    else if (p_Key == m_KeyToSelect)
    {
      int headerField = GetCurrentHeaderField();
      if ((headerField == HeaderTo) || (headerField == HeaderCc) || (headerField == HeaderBcc))
      {
        SetState(StateAddressList);
      }
      else if (headerField == HeaderFrom)
      {
        SetState(StateFromAddressList);
      }
      else if (headerField == HeaderAtt)
      {
        FilePickerOrStateFileList();
      }
    }
    else if ((p_Key == KEY_LEFT) && (m_ComposeHeaderPos == 0))
    {
      --m_ComposeHeaderLine;
      if (m_ComposeHeaderLine < 0)
      {
        m_ComposeHeaderPos = 0;
      }
      else
      {
        m_ComposeHeaderPos = std::numeric_limits<int>::max();
      }

      m_ComposeHeaderLine = Util::Bound(0, m_ComposeHeaderLine,
                                        (int)m_ComposeHeaderStr.size() - 1);
      m_ComposeHeaderPos = Util::Bound(0, m_ComposeHeaderPos,
                                       (int)m_ComposeHeaderStr.at(m_ComposeHeaderLine).size());
    }
    else if ((p_Key == KEY_RIGHT) && (m_ComposeHeaderPos == (int)m_ComposeHeaderStr.at(m_ComposeHeaderLine).size()))
    {
      m_ComposeHeaderPos = 0;

      if (m_ComposeHeaderLine < ((int)m_ComposeHeaderStr.size() - 1))
      {
        m_ComposeHeaderLine = Util::Bound(0, m_ComposeHeaderLine + 1,
                                          (int)m_ComposeHeaderStr.size() - 1);
        m_ComposeHeaderPos = Util::Bound(0, m_ComposeHeaderPos,
                                         (int)m_ComposeHeaderStr.at(m_ComposeHeaderLine).size());
      }
      else
      {
        m_IsComposeHeader = false;
        m_ComposeMessagePos = 0;
      }
    }
    else if (HandleComposeKey(p_Key))
    {
      // none
    }
    else if (HandleLineKey(p_Key, m_ComposeHeaderStr[m_ComposeHeaderLine], m_ComposeHeaderPos))
    {
      // none
    }
    else if (HandleDocKey(p_Key, m_ComposeHeaderStr[m_ComposeHeaderLine], m_ComposeHeaderPos))
    {
      // none
    }
    else
    {
      continueProcess = true;
    }
  }
  else // compose body
  {
    if (p_Key == KEY_UP)
    {
      ComposeMessagePrevLine();
    }
    else if (p_Key == KEY_DOWN)
    {
      ComposeMessageNextLine();
    }
    else if (p_Key == m_KeyPrevPageCompose)
    {
      const bool processFlowed = false; // only process when viewing message
      const bool outputFlowed = false; // only generate when sending after compose
      const bool quoteWrap = false; // only wrap quoted lines when viewing message
      const int expandTabSize = 0; // disabled
      for (int i = 0; i < (m_MainWinHeight / 2); ++i)
      {
        ComposeMessagePrevLine();
        m_ComposeMessageLines = Util::WordWrap(m_ComposeMessageStr, m_MaxComposeLineLength,
                                               processFlowed, outputFlowed, quoteWrap, expandTabSize,
                                               m_ComposeMessagePos, m_ComposeMessageWrapLine,
                                               m_ComposeMessageWrapPos);
      }
    }
    else if (p_Key == m_KeyNextPageCompose)
    {
      const bool processFlowed = false; // only process when viewing message
      const bool outputFlowed = false; // only generate when sending after compose
      const bool quoteWrap = false; // only wrap quoted lines when viewing message
      const int expandTabSize = 0; // disabled
      for (int i = 0; i < (m_MainWinHeight / 2); ++i)
      {
        ComposeMessageNextLine();
        m_ComposeMessageLines = Util::WordWrap(m_ComposeMessageStr, m_MaxComposeLineLength,
                                               processFlowed, outputFlowed, quoteWrap, expandTabSize,
                                               m_ComposeMessagePos, m_ComposeMessageWrapLine,
                                               m_ComposeMessageWrapPos);
      }
    }
    else if ((p_Key == KEY_LEFT) && (m_ComposeMessagePos == 0))
    {
      // switch to header
      m_IsComposeHeader = true;
      m_ComposeHeaderPos = (int)m_ComposeHeaderStr.at(m_ComposeHeaderLine).size();
    }
    else if (p_Key == KEY_TAB)
    {
      DrawAll(); // redraw to update current column position (m_ComposeMessageWrapPos)
      int tabSpaces = (m_TabSize - (m_ComposeMessageWrapPos % m_TabSize));
      for (int i = 0; i < tabSpaces; ++i)
      {
        m_ComposeMessageStr.insert(m_ComposeMessagePos++, 1, ' ');
      }

      asyncRedraw = true;
    }
    else if (HandleComposeKey(p_Key))
    {
      // none
    }
    else if (HandleLineKey(p_Key, m_ComposeMessageStr, m_ComposeMessagePos))
    {
      // none
    }
    else if (HandleDocKey(p_Key, m_ComposeMessageStr, m_ComposeMessagePos))
    {
      // none
    }
    else
    {
      continueProcess = true;
    }
  }

  // process common key handling last (if not handled above)
  if (continueProcess)
  {
    if (IsValidTextKey(p_Key))
    {
      if (m_IsComposeHeader)
      {
        m_ComposeHeaderStr[m_ComposeHeaderLine].insert(m_ComposeHeaderPos++, 1, p_Key);
      }
      else
      {
        m_ComposeMessageStr.insert(m_ComposeMessagePos++, 1, p_Key);
      }

      asyncRedraw = true;
    }
    else if (m_InvalidInputNotify)
    {
      SetDialogMessage("Invalid input (" + Util::ToHexString(p_Key) + ")");
    }
  }

  if (asyncRedraw)
  {
    AsyncUiRequest(UiRequestDrawAll);
  }
  else
  {
    DrawAll();
  }
}

void Ui::ViewPartListKeyHandler(int p_Key)
{
  if (p_Key == m_KeyQuit)
  {
    Quit();
  }
  else if ((p_Key == KEY_BACKSPACE) || (p_Key == KEY_DELETE) || (p_Key == m_KeyBack) || (p_Key == KEY_LEFT))
  {
    const std::string& attachmentsTempDir = Util::GetAttachmentsTempDir();
    LOG_DEBUG("deleting %s", attachmentsTempDir.c_str());
    Util::CleanupAttachmentsTempDir();
    SetState(StateViewMessage);
  }
  else if ((p_Key == KEY_RETURN) || (p_Key == KEY_ENTER) || (p_Key == m_KeyOpen) || (p_Key == KEY_RIGHT))
  {
    std::string ext;
    std::string err;
    std::string fileName;
    bool isUnamedTextHtml = false;
    if (!m_PartListCurrentPartInfo.m_Filename.empty())
    {
      ext = Util::GetFileExt(m_PartListCurrentPartInfo.m_Filename);
      fileName = m_PartListCurrentPartInfo.m_Filename;
      if (ext.empty())
      {
        LOG_DEBUG("cannot determine file extension for %s", m_PartListCurrentPartInfo.m_Filename.c_str());
      }
    }
    else
    {
      ext = Util::ExtensionForMimeType(m_PartListCurrentPartInfo.m_MimeType);
      fileName = std::to_string(m_PartListCurrentIndex) + ext;
      isUnamedTextHtml = (m_PartListCurrentPartInfo.m_MimeType == "text/html");
      if (ext.empty())
      {
        LOG_DEBUG("no file extension for MIME type %s", m_PartListCurrentPartInfo.m_MimeType.c_str());
      }
    }

    std::string tempFilePath;
    std::string partData;

    {
      std::lock_guard<std::mutex> lock(m_Mutex);
      const std::string& folder = m_CurrentFolderUid.first;
      const int uid = m_CurrentFolderUid.second;
      std::map<uint32_t, Body>& bodys = m_Bodys[folder];
      std::map<uint32_t, Body>::iterator bodyIt = bodys.find(uid);
      if (bodyIt != bodys.end())
      {
        Body& body = bodyIt->second;
        const std::map<ssize_t, PartInfo>& parts = body.GetPartInfos();
        const std::map<ssize_t, std::string>& partDatas = body.GetPartDatas();
        partData = partDatas.at(m_PartListCurrentIndex);

        if (m_ShowEmbeddedImages && isUnamedTextHtml)
        {
          for (auto& part : parts)
          {
            if (!part.second.m_ContentId.empty())
            {
              const std::string& tempPartFilePath = Util::GetAttachmentsTempDir() + part.second.m_ContentId;
              LOG_DEBUG("writing \"%s\"", tempPartFilePath.c_str());
              Util::WriteFile(tempPartFilePath, partDatas.at(part.first));
            }
          }
        }
      }

      if (m_ShowEmbeddedImages && isUnamedTextHtml)
      {
        tempFilePath = Util::GetAttachmentsTempDir() + fileName;
        Util::ReplaceString(partData, "src=cid:", "src=file://" + Util::GetAttachmentsTempDir());
        Util::ReplaceString(partData, "src=\"cid:", "src=\"file://" + Util::GetAttachmentsTempDir());
        LOG_DEBUG("writing \"%s\"", tempFilePath.c_str());
        Util::WriteFile(tempFilePath, partData);
      }
      else
      {
        tempFilePath = Util::GetAttachmentsTempDir() + fileName;
        LOG_DEBUG("writing \"%s\"", tempFilePath.c_str());
        Util::WriteFile(tempFilePath, partData);
      }
    }

    LOG_DEBUG("opening \"%s\" in external viewer", tempFilePath.c_str());

    SetDialogMessage("Waiting for external viewer to exit");
    DrawDialog();
    int rv = ExtPartsViewer(tempFilePath);
    if (rv != 0)
    {
      SetDialogMessage("External viewer error code " + std::to_string(rv), true /* p_Warn */);
    }
    else
    {
      LOG_DEBUG("external viewer exited successfully");
      SetDialogMessage("");
    }
  }
  else if (p_Key == m_KeySaveFile)
  {
    std::string filename = Util::GetDownloadsDir() + m_PartListCurrentPartInfo.m_Filename;
    if (PromptString("Save Filename: ", "Save", filename))
    {
      if (!filename.empty())
      {
        filename = Util::ExpandPath(filename);

        std::string partData;
        {
          std::lock_guard<std::mutex> lock(m_Mutex);
          const std::string& folder = m_CurrentFolderUid.first;
          const int uid = m_CurrentFolderUid.second;
          std::map<uint32_t, Body>& bodys = m_Bodys[folder];
          std::map<uint32_t, Body>::iterator bodyIt = bodys.find(uid);
          if (bodyIt != bodys.end())
          {
            Body& body = bodyIt->second;
            const std::map<ssize_t, std::string>& partDatas = body.GetPartDatas();
            partData = partDatas.at(m_PartListCurrentIndex);
          }
        }

        Util::WriteFile(filename, partData);
        SetDialogMessage("File saved");
      }
      else
      {
        SetDialogMessage("Save cancelled (empty filename)");
      }
    }
    else
    {
      SetDialogMessage("Save cancelled");
    }
  }
  else if (p_Key == m_KeyGotoInbox)
  {
    if (m_MessageListSearch)
    {
      m_MessageListSearch = false;
      m_PreviousFolder = "";
    }

    m_CurrentFolder = m_Inbox;
    SetState(StateViewMessageList);
  }
  else if (HandleListKey(p_Key, m_PartListCurrentIndex))
  {
    // none
  }
  else if (m_InvalidInputNotify)
  {
    SetDialogMessage("Invalid input (" + Util::ToHexString(p_Key) + ")");
  }

  DrawAll();
}

void Ui::SetState(Ui::State p_State)
{
  if ((p_State == StateAddressList) || (p_State == StateFromAddressList) || (p_State == StateFileList))
  {
    // going to address or file list
    m_LastMessageState = m_State;
    m_State = p_State;
  }
  else if ((m_State != StateAddressList) && (m_State != StateFromAddressList) && (m_State != StateFileList))
  {
    // normal state transition
    m_LastState = m_State;
    m_State = p_State;
  }
  else
  {
    // exiting address or file list
    m_State = p_State;
    return;
  }

  if (m_State == StateGotoFolder)
  {
    curs_set(1);
    m_FolderListFilterPos = 0;
    m_FolderListFilterStr.clear();
    m_FolderListCurrentFolder = m_CurrentFolder;
    m_FolderListCurrentIndex = INT_MAX;
  }
  else if (m_State == StateMoveToFolder)
  {
    curs_set(1);
    if (m_IsAutoMove)
    {
      AutoMoveSelectFolder();
    }
    else if (m_PersistFolderFilter)
    {
      m_FolderListFilterPos = m_PersistedFolderListFilterPos;
      m_FolderListFilterStr = m_PersistedFolderListFilterStr;
      if (!m_PersistedFolderListCurrentFolder.empty())
      {
        m_FolderListCurrentFolder = m_PersistedFolderListCurrentFolder;
        m_FolderListCurrentIndex = m_PersistedFolderListCurrentIndex;
      }
      else
      {
        m_FolderListCurrentFolder = m_CurrentFolder;
        m_FolderListCurrentIndex = INT_MAX;
      }
    }
    else
    {
      m_FolderListFilterPos = 0;
      m_FolderListFilterStr.clear();
      m_FolderListCurrentFolder = m_CurrentFolder;
      m_FolderListCurrentIndex = INT_MAX;
    }
  }
  else if (m_State == StateViewMessageList)
  {
    curs_set(0);
    m_HelpViewMessagesListOffset = 0;
    {
      std::lock_guard<std::mutex> lock(m_Mutex);
      m_MessageViewToggledSeen = false;
    }
  }
  else if (m_State == StateViewMessage)
  {
    curs_set(0);
    m_MessageViewLineOffset = 0;
    m_HelpViewMessageOffset = 0;
    m_MessageFindMatchLine = -1;
  }
  else if ((m_State == StateComposeMessage) || (m_State == StateComposeCopyMessage))
  {
    curs_set(1);
    SetComposeStr(HeaderAll, L"");
    SetComposeStr(HeaderFrom, Util::ToWString(GetDefaultFrom()));
    StartComposeBackup();
    m_ComposeHeaderLine = m_ShowRichHeader ? 1 : 0;
    m_ComposeHeaderPos = 0;
    m_ComposeHeaderRef.clear();
    m_ComposeMessageStr.clear();
    m_ComposeMessagePos = 0;
    m_IsComposeHeader = true;
    m_ComposeDraftUid = 0;
    m_ComposeMessageOffsetY = 0;
    m_ComposeTempDirectory.clear();
    m_CurrentMarkdownHtmlCompose = m_MarkdownHtmlCompose;
    m_ComposeQuotedStart.clear();

    std::lock_guard<std::mutex> lock(m_Mutex);
    const std::string& folder = m_CurrentFolderUid.first;
    const int uid = m_CurrentFolderUid.second;

    if ((folder == m_DraftsFolder) || (m_State == StateComposeCopyMessage))
    {
      std::map<uint32_t, Header>& headers = m_Headers[folder];
      std::map<uint32_t, Body>& bodys = m_Bodys[folder];

      std::map<uint32_t, Header>::iterator hit = headers.find(uid);
      std::map<uint32_t, Body>::iterator bit = bodys.find(uid);
      if ((hit != headers.end()) && (bit != bodys.end()))
      {
        m_ComposeDraftUid = (m_State != StateComposeCopyMessage) ? uid : 0;

        Header& header = hit->second;
        Body& body = bit->second;

        std::string bodyText = body.GetTextPlain();
        if ((folder != m_DraftsFolder) && !bodyText.empty() && bodyText[bodyText.size() - 1] == '\n')
        {
          bodyText = bodyText.substr(0, bodyText.size() - 1);
        }

        m_ComposeMessageStr = Util::ToWString(bodyText);
        Util::StripCR(m_ComposeMessageStr);

        SetComposeStr(HeaderFrom, Util::ToWString(header.GetFrom()));
        SetComposeStr(HeaderTo, Util::ToWString(header.GetTo()));
        SetComposeStr(HeaderCc, Util::ToWString(header.GetCc()));
        SetComposeStr(HeaderBcc, Util::ToWString(header.GetBcc()));
        SetComposeStr(HeaderAtt, L"");
        SetComposeStr(HeaderSub, Util::ToWString(header.GetSubject()));

        if (!GetComposeStr(HeaderBcc).empty())
        {
          m_ShowRichHeader = true;
        }

        int idx = 0;
        std::string tmppath = Util::GetTempDirectory();
        const std::map<ssize_t, std::string>& partDatas = body.GetPartDatas();
        for (auto& part : body.GetPartInfos())
        {
          if (!part.second.m_Filename.empty())
          {
            std::string tmpfiledir = tmppath + "/" + std::to_string(idx++) + "/";
            Util::MkDir(tmpfiledir);
            std::string tmpfilepath = tmpfiledir + part.second.m_Filename;

            Util::WriteFile(tmpfilepath, partDatas.at(part.first));
            tmpfilepath = Util::EscapePath(tmpfilepath);
            if (GetComposeStr(HeaderAtt).empty())
            {
              SetComposeStr(HeaderAtt, GetComposeStr(HeaderAtt) + Util::ToWString(tmpfilepath));
            }
            else
            {
              SetComposeStr(HeaderAtt, GetComposeStr(HeaderAtt) + L", " + Util::ToWString(tmpfilepath));
            }
          }
        }

        m_ComposeHeaderRef = header.GetMessageId();
        m_ComposeTempDirectory = tmppath;
      }
    }
    else
    {
      m_ComposeMessageStr = GetSignatureStr();
    }
  }
  else if ((m_State == StateReplyAllMessage) || (m_State == StateReplySenderMessage))
  {
    curs_set(1);
    SetComposeStr(HeaderAll, L"");
    SetComposeStr(HeaderFrom, Util::ToWString(GetDefaultFrom()));
    StartComposeBackup();
    m_ComposeHeaderLine = m_ShowRichHeader ? 5 : 3;
    m_ComposeHeaderPos = 0;
    m_ComposeMessageStr.clear();
    m_ComposeMessagePos = 0;
    m_ComposeMessageOffsetY = 0;
    m_ComposeTempDirectory.clear();
    m_CurrentMarkdownHtmlCompose = m_MarkdownHtmlCompose;
    m_ComposeQuotedStart.clear();

    std::lock_guard<std::mutex> lock(m_Mutex);
    const std::string& folder = m_CurrentFolderUid.first;
    const int uid = m_CurrentFolderUid.second;

    std::map<uint32_t, Header>& headers = m_Headers[folder];
    std::map<uint32_t, Body>& bodys = m_Bodys[folder];

    std::map<uint32_t, Header>::iterator hit = headers.find(uid);
    std::map<uint32_t, Body>::iterator bit = bodys.find(uid);
    if ((hit != headers.end()) && (bit != bodys.end()))
    {
      Header& header = hit->second;
      Body& body = bit->second;

      std::string bodyText = GetBodyText(body);
      if (!bodyText.empty() && bodyText[bodyText.size() - 1] == '\n')
      {
        bodyText = bodyText.substr(0, bodyText.size() - 1);
      }

      std::string indentBodyText = Util::AddIndent(bodyText, "> ");
      std::string indentBody;
      if (m_ComposeLineWrap == LineWrapFormatFlowed)
      {
        indentBody = indentBodyText;
      }
      else
      {
        const bool processFlowed = m_RespectFormatFlowed && m_Plaintext && body.IsFormatFlowed();
        const bool outputFlowed = false;
        const bool quoteWrap = m_RewrapQuotedLines;
        const int expandTabSize = m_TabSize; // enabled
        std::vector<std::wstring> indentBodyLines =
          Util::WordWrap(Util::ToWString(indentBodyText), 72, processFlowed, outputFlowed, quoteWrap, expandTabSize);
        indentBody = Util::ToString(Util::Join(indentBodyLines));
      }

      m_ComposeQuotedStart = "On " + header.GetDateTime() + " " + header.GetFrom() + " wrote:\n\n";
      if (!m_BottomReply)
      {
        m_ComposeMessageStr = GetSignatureStr() +
          Util::ToWString("\n\n" + m_ComposeQuotedStart + indentBody);
        Util::StripCR(m_ComposeMessageStr);
      }
      else
      {
        m_ComposeMessageStr =
          Util::ToWString(m_ComposeQuotedStart + indentBody + "\n\n\n");
        Util::StripCR(m_ComposeMessageStr);
        m_ComposeMessagePos = (int)m_ComposeMessageStr.size() - 1;
        int lineCount = Util::Split(Util::ToString(m_ComposeMessageStr), '\n').size();
        m_ComposeMessageOffsetY = std::max(lineCount - (m_MainWinHeight / 2), 0);
        m_ComposeMessageStr += GetSignatureStr();
      }

      {
        if (folder == m_SentFolder)
        {
          SetComposeStr(HeaderTo, Util::ToWString(header.GetTo()));
        }
        else
        {
          if (!header.GetReplyTo().empty())
          {
            SetComposeStr(HeaderTo, Util::ToWString(header.GetReplyTo()));
          }
          else
          {
            SetComposeStr(HeaderTo, Util::ToWString(header.GetFrom()));
          }
        }

        if (m_State == StateReplyAllMessage)
        {
          std::vector<std::string> tos = Util::SplitAddrs(header.GetTo());
          std::vector<std::string> ccs = Util::SplitAddrs(header.GetCc());

          if (folder != m_SentFolder)
          {
            ccs.insert(ccs.end(), tos.begin(), tos.end());
          }

          std::string selfAddress = m_SmtpManager->GetAddress();
          for (auto it = ccs.begin(); it != ccs.end(); /* incremented in loop */)
          {
            it = ((it->find(selfAddress) == std::string::npos) &&
                  (it->find(header.GetFrom()) == std::string::npos)) ? std::next(it) : ccs.erase(it);
          }

          SetComposeStr(HeaderCc, Util::ToWString(Util::Join(ccs, ", ")));
        }
      }

      SetComposeStr(HeaderBcc, L"");
      SetComposeStr(HeaderAtt, L"");
      SetComposeStr(HeaderSub, Util::ToWString(Util::MakeReplySubject(header.GetSubject())));

      m_ComposeHeaderRef = header.GetMessageId();
    }

    m_IsComposeHeader = false;
  }
  else if (m_State == StateForwardMessage)
  {
    curs_set(1);
    SetComposeStr(HeaderAll, L"");
    SetComposeStr(HeaderFrom, Util::ToWString(GetDefaultFrom()));
    StartComposeBackup();
    m_ComposeHeaderLine = m_ShowRichHeader ? 1 : 0;
    m_ComposeHeaderPos = 0;
    m_ComposeMessageStr.clear();
    m_ComposeMessagePos = 0;
    m_ComposeMessageOffsetY = 0;
    m_ComposeTempDirectory.clear();
    m_CurrentMarkdownHtmlCompose = m_MarkdownHtmlCompose;
    m_ComposeQuotedStart.clear();

    std::lock_guard<std::mutex> lock(m_Mutex);
    const std::string& folder = m_CurrentFolderUid.first;
    const int uid = m_CurrentFolderUid.second;

    std::map<uint32_t, Header>& headers = m_Headers[folder];
    std::map<uint32_t, Body>& bodys = m_Bodys[folder];

    std::map<uint32_t, Header>::iterator hit = headers.find(uid);
    std::map<uint32_t, Body>::iterator bit = bodys.find(uid);
    if ((hit != headers.end()) && (bit != bodys.end()))
    {
      Header& header = hit->second;
      Body& body = bit->second;

      int idx = 0;
      std::string tmppath = Util::GetTempDirectory();
      const std::map<ssize_t, std::string>& partDatas = body.GetPartDatas();
      for (auto& part : body.GetPartInfos())
      {
        if (!part.second.m_Filename.empty())
        {
          std::string tmpfiledir = tmppath + "/" + std::to_string(idx++) + "/";
          Util::MkDir(tmpfiledir);
          std::string tmpfilepath = tmpfiledir + part.second.m_Filename;

          Util::WriteFile(tmpfilepath, partDatas.at(part.first));
          tmpfilepath = Util::EscapePath(tmpfilepath);
          if (GetComposeStr(HeaderAtt).empty())
          {
            SetComposeStr(HeaderAtt,
                          GetComposeStr(HeaderAtt) + Util::ToWString(tmpfilepath));
          }
          else
          {
            SetComposeStr(HeaderAtt,
                          GetComposeStr(HeaderAtt) + L", " + Util::ToWString(tmpfilepath));
          }
        }
      }

      m_ComposeQuotedStart = "\n\n---------- Forwarded message ---------\n";
      m_ComposeMessageStr =
        GetSignatureStr() +
        Util::ToWString(m_ComposeQuotedStart +
                        "From: " + header.GetFrom() + "\n"
                        "Date: " + header.GetDateTime() + "\n"
                        "Subject: " + header.GetSubject() + "\n"
                        "To: " + header.GetTo() + "\n");
      if (!header.GetReplyTo().empty())
      {
        m_ComposeMessageStr +=
          Util::ToWString("Reply-To: " + header.GetReplyTo() + "\n");
      }

      if (!header.GetCc().empty())
      {
        m_ComposeMessageStr +=
          Util::ToWString("Cc: " + header.GetCc() + "\n");
      }

      std::string bodyText = GetBodyText(body);
      if (!bodyText.empty() && bodyText[bodyText.size() - 1] == '\n')
      {
        bodyText = bodyText.substr(0, bodyText.size() - 1);
      }

      m_ComposeMessageStr += Util::ToWString("\n" + bodyText);
      Util::StripCR(m_ComposeMessageStr);

      SetComposeStr(HeaderSub, Util::ToWString(Util::MakeForwardSubject(header.GetSubject())));

      m_ComposeHeaderRef = header.GetMessageId();
      m_ComposeTempDirectory = tmppath;
    }

    m_IsComposeHeader = true;
  }
  else if (m_State == StateForwardAttachedMessage)
  {
    curs_set(1);
    SetComposeStr(HeaderAll, L"");
    SetComposeStr(HeaderFrom, Util::ToWString(GetDefaultFrom()));
    StartComposeBackup();
    m_ComposeHeaderLine = m_ShowRichHeader ? 1 : 0;
    m_ComposeHeaderPos = 0;
    m_ComposeMessageStr.clear();
    m_ComposeMessagePos = 0;
    m_ComposeMessageOffsetY = 0;
    m_ComposeTempDirectory.clear();
    m_CurrentMarkdownHtmlCompose = m_MarkdownHtmlCompose;
    m_ComposeQuotedStart.clear();

    std::lock_guard<std::mutex> lock(m_Mutex);
    const std::string& folder = m_CurrentFolderUid.first;
    const int uid = m_CurrentFolderUid.second;

    std::map<uint32_t, Header>& headers = m_Headers[folder];
    std::map<uint32_t, Body>& bodys = m_Bodys[folder];

    std::map<uint32_t, Header>::iterator hit = headers.find(uid);
    std::map<uint32_t, Body>::iterator bit = bodys.find(uid);
    if ((hit != headers.end()) && (bit != bodys.end()))
    {
      Header& header = hit->second;
      Body& body = bit->second;

      std::string tmppath = Util::GetTempDirectory();
      std::string filename = header.GetSubject();
      Util::RemoveNonAlphaNumSpace(filename);
      Util::ReplaceString(filename, " ", "_");
      std::string filepath = tmppath + "/" + filename + ".eml";
      LOG_INFO("write to %s size %d", filepath.c_str(), body.GetData().size());
      Util::WriteFile(filepath, body.GetData());

      SetComposeStr(HeaderAtt,
                    GetComposeStr(HeaderAtt) + Util::ToWString(filepath));

      SetComposeStr(HeaderSub, Util::ToWString(Util::MakeForwardSubject(header.GetSubject())));

      m_ComposeHeaderRef = header.GetMessageId();
      m_ComposeTempDirectory = tmppath;
    }

    m_IsComposeHeader = true;
  }
  else if ((m_State == StateAddressList) || (m_State == StateFromAddressList))
  {
    curs_set(1);
    m_AddressListFilterPos = 0;
    m_AddressListFilterStr.clear();
    m_Addresses = (m_State == StateAddressList) ? AddressBook::Get("") : AddressBook::GetFrom("");
    m_AddressListCurrentIndex = 0;
    m_AddressListCurrentAddress = "";
  }
  else if (m_State == StateFileList)
  {
    curs_set(1);
    m_FileListFilterPos = 0;
    m_FileListFilterStr.clear();
    if (m_CurrentDir.empty() || !m_PersistFileSelectionDir)
    {
      m_CurrentDir = Util::GetCurrentWorkingDir();
    }

    m_Files = Util::ListPaths(m_CurrentDir);
    m_FileListCurrentIndex = 0;
    m_FileListCurrentFile.m_Name = "";
  }
  else if (m_State == StateViewPartList)
  {
    curs_set(0);
    m_PartListCurrentIndex = 0;
  }
}

void Ui::ResponseHandler(const ImapManager::Request& p_Request, const ImapManager::Response& p_Response)
{
  if (!s_Running) return;

  char uiRequest = UiRequestNone;

  bool updateIndexFromUid = false;

  if (p_Request.m_PrefetchLevel < PrefetchLevelFullSync)
  {
    std::set<uint32_t> fetchHeaderUids;
    std::set<uint32_t> fetchFlagUids;

    if (p_Request.m_GetFolders && !(p_Response.m_ResponseStatus & ImapManager::ResponseStatusGetFoldersFailed))
    {
      std::lock_guard<std::mutex> lock(m_Mutex);
      m_Folders = p_Response.m_Folders;
      uiRequest |= UiRequestDrawAll;
      LOG_DEBUG_VAR("new folders =", p_Response.m_Folders);
    }

    if (p_Request.m_GetUids && !(p_Response.m_ResponseStatus & ImapManager::ResponseStatusGetUidsFailed))
    {
      std::lock_guard<std::mutex> lock(m_Mutex);

      const std::set<uint32_t> newUids = p_Response.m_Uids - m_Uids[p_Response.m_Folder];
      if (!p_Response.m_Cached && (p_Response.m_Folder == m_Inbox) && !newUids.empty())
      {
        if (m_NewMsgBell)
        {
          LOG_DEBUG("bell");
          beep();
        }
      }

      const std::set<uint32_t>& removedUids = m_Uids[p_Response.m_Folder] - p_Response.m_Uids;
      if (!removedUids.empty())
      {
        LOG_DEBUG_VAR("del uids =", removedUids);
        UpdateDisplayUids(p_Response.m_Folder, removedUids);
        m_Headers[p_Response.m_Folder] = m_Headers[p_Response.m_Folder] - removedUids;
      }

      m_Uids[p_Response.m_Folder] = p_Response.m_Uids;
      uiRequest |= UiRequestDrawAll;
      updateIndexFromUid = true;
      LOG_DEBUG_VAR("new uids =", newUids);

      if (!m_PrefetchAllHeaders && !newUids.empty())
      {
        UpdateDisplayUids(p_Response.m_Folder, std::set<uint32_t>(), newUids);
      }

      if (m_PrefetchAllHeaders)
      {
        std::map<uint32_t, Header>& headers = m_Headers[p_Response.m_Folder];
        std::map<uint32_t, uint32_t>& flags = m_Flags[p_Response.m_Folder];
        std::set<uint32_t>& requestedHeaders = m_RequestedHeaders[p_Response.m_Folder];
        std::set<uint32_t>& requestedFlags = m_RequestedFlags[p_Response.m_Folder];
        for (auto& uid : newUids)
        {
          if ((headers.find(uid) == headers.end()) &&
              (requestedHeaders.find(uid) == requestedHeaders.end()))
          {
            fetchHeaderUids.insert(uid);
            requestedHeaders.insert(uid);
          }
        }

        for (auto& uid : p_Response.m_Uids)
        {
          if (((flags.find(uid) == flags.end()) &&
               (requestedFlags.find(uid) == requestedFlags.end())))
          {
            fetchFlagUids.insert(uid);
            requestedFlags.insert(uid);
          }
        }
      }
    }

    if (!p_Request.m_GetHeaders.empty() &&
        !(p_Response.m_ResponseStatus & ImapManager::ResponseStatusGetHeadersFailed))
    {
      std::lock_guard<std::mutex> lock(m_Mutex);

      const std::map<uint32_t, Header>& headers = p_Response.m_Headers;

      m_Headers[p_Response.m_Folder].insert(headers.begin(), headers.end());
      if (m_PrefetchAllHeaders)
      {
        UpdateDisplayUids(p_Response.m_Folder, std::set<uint32_t>(), MapKey(headers));
      }
      uiRequest |= UiRequestDrawAll;
      updateIndexFromUid = true;
      LOG_DEBUG_VAR("new headers =", MapKey(headers));
    }

    if (!p_Request.m_GetFlags.empty() &&
        !(p_Response.m_ResponseStatus & ImapManager::ResponseStatusGetFlagsFailed))
    {
      std::lock_guard<std::mutex> lock(m_Mutex);
      std::map<uint32_t, uint32_t> newFlags = p_Response.m_Flags;
      newFlags.insert(m_Flags[p_Response.m_Folder].begin(), m_Flags[p_Response.m_Folder].end());
      m_Flags[p_Response.m_Folder] = newFlags;
      uiRequest |= UiRequestDrawAll;
      LOG_DEBUG_VAR("new flags =", MapKey(p_Response.m_Flags));
    }

    if (!p_Request.m_GetBodys.empty() &&
        !(p_Response.m_ResponseStatus & ImapManager::ResponseStatusGetBodysFailed))
    {
      std::lock_guard<std::mutex> lock(m_Mutex);
      m_Bodys[p_Response.m_Folder].insert(p_Response.m_Bodys.begin(), p_Response.m_Bodys.end());
      uiRequest |= UiRequestDrawAll;
      LOG_DEBUG_VAR("new bodys =", MapKey(p_Response.m_Bodys));
    }

    // perform fetch
    if (!fetchHeaderUids.empty())
    {
      const int maxHeadersFetchRequest = 25;
      std::set<uint32_t> subsetFetchHeaderUids;
      for (auto it = fetchHeaderUids.begin(); it != fetchHeaderUids.end(); ++it)
      {
        subsetFetchHeaderUids.insert(*it);
        if ((subsetFetchHeaderUids.size() == maxHeadersFetchRequest) ||
            (std::next(it) == fetchHeaderUids.end()))
        {
          ImapManager::Request request;
          request.m_Folder = p_Response.m_Folder;
          request.m_GetHeaders = subsetFetchHeaderUids;

          LOG_DEBUG_VAR("async req headers =", subsetFetchHeaderUids);
          m_ImapManager->AsyncRequest(request);

          subsetFetchHeaderUids.clear();
        }
      }
    }

    if (!fetchFlagUids.empty())
    {
      const int maxFlagsFetchRequest = 1000;
      std::set<uint32_t> subsetFetchFlagUids;
      for (auto it = fetchFlagUids.begin(); it != fetchFlagUids.end(); ++it)
      {
        subsetFetchFlagUids.insert(*it);
        if ((subsetFetchFlagUids.size() == maxFlagsFetchRequest) ||
            (std::next(it) == fetchFlagUids.end()))
        {
          ImapManager::Request request;
          request.m_Folder = p_Response.m_Folder;
          request.m_GetFlags = subsetFetchFlagUids;

          LOG_DEBUG_VAR("async req flags =", subsetFetchFlagUids);
          m_ImapManager->AsyncRequest(request);

          subsetFetchFlagUids.clear();
        }
      }
    }
  }

  if (p_Request.m_PrefetchLevel == PrefetchLevelFullSync)
  {
    if (p_Request.m_GetFolders &&
        !(p_Response.m_ResponseStatus & ImapManager::ResponseStatusGetFoldersFailed))
    {
      std::lock_guard<std::mutex> lock(m_Mutex);
      for (auto& folder : p_Response.m_Folders)
      {
        if (!s_Running)
        {
          break;
        }

        if (!m_HasPrefetchRequestedUids[folder])
        {
          ImapManager::Request request;
          request.m_PrefetchLevel = PrefetchLevelFullSync;
          request.m_Folder = folder;
          request.m_GetUids = true;
          LOG_DEBUG_VAR("prefetch req uids =", folder);
          m_HasPrefetchRequestedUids[folder] = true;
          m_ImapManager->PrefetchRequest(request);
        }
      }
    }

    if (p_Request.m_GetUids &&
        !(p_Response.m_ResponseStatus & ImapManager::ResponseStatusGetUidsFailed))
    {
      const std::string& folder = p_Response.m_Folder;

      std::set<uint32_t> prefetchHeaders;
      std::set<uint32_t> prefetchFlags;
      std::set<uint32_t> prefetchBodys;

      {
        std::lock_guard<std::mutex> lock(m_Mutex);

        std::map<uint32_t, Header>& headers = m_Headers[folder];
        std::set<uint32_t>& requestedHeaders = m_RequestedHeaders[folder];
        std::set<uint32_t>& prefetchedHeaders = m_PrefetchedHeaders[folder];

        std::map<uint32_t, uint32_t>& flags = m_Flags[folder];
        std::set<uint32_t>& requestedFlags = m_RequestedFlags[folder];
        std::set<uint32_t>& prefetchedFlags = m_PrefetchedFlags[folder];

        std::map<uint32_t, Body>& bodys = m_Bodys[folder];
        std::set<uint32_t>& requestedBodys = m_RequestedBodys[folder];
        std::set<uint32_t>& prefetchedBodys = m_PrefetchedBodys[folder];

        for (auto& uid : p_Response.m_Uids)
        {
          if ((headers.find(uid) == headers.end()) &&
              (requestedHeaders.find(uid) == requestedHeaders.end()) &&
              (prefetchedHeaders.find(uid) == prefetchedHeaders.end()))
          {
            prefetchHeaders.insert(uid);
            prefetchedHeaders.insert(uid);
          }

          if ((flags.find(uid) == flags.end()) &&
              (requestedFlags.find(uid) == requestedFlags.end()) &&
              (prefetchedFlags.find(uid) == prefetchedFlags.end()))
          {
            prefetchFlags.insert(uid);
            prefetchedFlags.insert(uid);
          }

          if ((bodys.find(uid) == bodys.end()) &&
              (requestedBodys.find(uid) == requestedBodys.end()) &&
              (prefetchedBodys.find(uid) == prefetchedBodys.end()))
          {
            prefetchBodys.insert(uid);
            prefetchedBodys.insert(uid);
          }
        }
      }

      const int maxHeadersFetchRequest = 25;
      if (!prefetchHeaders.empty())
      {
        std::set<uint32_t> subsetPrefetchHeaders;
        for (auto it = prefetchHeaders.begin(); it != prefetchHeaders.end(); ++it)
        {
          if (!s_Running) break;

          subsetPrefetchHeaders.insert(*it);
          if ((subsetPrefetchHeaders.size() == maxHeadersFetchRequest) ||
              (std::next(it) == prefetchHeaders.end()))
          {
            ImapManager::Request request;
            request.m_PrefetchLevel = PrefetchLevelFullSync;
            request.m_Folder = folder;
            request.m_GetHeaders = subsetPrefetchHeaders;

            LOG_DEBUG_VAR("prefetch req headers =", subsetPrefetchHeaders);
            m_ImapManager->PrefetchRequest(request);

            subsetPrefetchHeaders.clear();
          }
        }
      }

      const int maxFlagsFetchRequest = 1000;
      if (!prefetchFlags.empty())
      {
        std::set<uint32_t> subsetPrefetchFlags;
        for (auto it = prefetchFlags.begin(); it != prefetchFlags.end(); ++it)
        {
          if (!s_Running) break;

          subsetPrefetchFlags.insert(*it);
          if ((subsetPrefetchFlags.size() == maxFlagsFetchRequest) ||
              (std::next(it) == prefetchFlags.end()))
          {
            ImapManager::Request request;
            request.m_PrefetchLevel = PrefetchLevelFullSync;
            request.m_Folder = folder;
            request.m_GetFlags = subsetPrefetchFlags;

            LOG_DEBUG_VAR("prefetch req flags =", subsetPrefetchFlags);
            m_ImapManager->PrefetchRequest(request);

            subsetPrefetchFlags.clear();
          }
        }
      }

      const int maxBodysFetchRequest = 1;
      if (!prefetchBodys.empty())
      {
        std::set<uint32_t> subsetPrefetchBodys;
        for (auto it = prefetchBodys.begin(); it != prefetchBodys.end(); ++it)
        {
          if (!s_Running) break;

          subsetPrefetchBodys.insert(*it);
          if ((subsetPrefetchBodys.size() == maxBodysFetchRequest) ||
              (std::next(it) == prefetchBodys.end()))
          {
            ImapManager::Request request;
            request.m_PrefetchLevel = PrefetchLevelFullSync;
            request.m_Folder = folder;
            request.m_GetBodys = subsetPrefetchBodys;

            LOG_DEBUG_VAR("prefetch req bodys =", subsetPrefetchBodys);
            m_ImapManager->PrefetchRequest(request);

            subsetPrefetchBodys.clear();
          }
        }
      }
    }
  }

  if (p_Response.m_ResponseStatus != ImapManager::ResponseStatusOk)
  {
    if (p_Response.m_ResponseStatus & ImapManager::ResponseStatusGetFoldersFailed)
    {
      SetDialogMessage("Get folders failed", true /* p_Warn */);
    }
    else if (p_Response.m_ResponseStatus & ImapManager::ResponseStatusGetBodysFailed)
    {
      SetDialogMessage("Get message body failed", true /* p_Warn */);
    }
    else if (p_Response.m_ResponseStatus & ImapManager::ResponseStatusGetHeadersFailed)
    {
      SetDialogMessage("Get message headers failed", true /* p_Warn */);
    }
    else if (p_Response.m_ResponseStatus & ImapManager::ResponseStatusGetUidsFailed)
    {
      SetDialogMessage("Get message ids failed", true /* p_Warn */);
    }
    else if (p_Response.m_ResponseStatus & ImapManager::ResponseStatusGetFlagsFailed)
    {
      SetDialogMessage("Get message flags failed", true /* p_Warn */);
    }
    else if (p_Response.m_ResponseStatus & ImapManager::ResponseStatusLoginFailed)
    {
      SetDialogMessage("Login failed", true /* p_Warn */);
    }
  }

  if (updateIndexFromUid)
  {
    UpdateIndexFromUid();
  }

  AsyncUiRequest(uiRequest);
}

void Ui::ResultHandler(const ImapManager::Action& p_Action, const ImapManager::Result& p_Result)
{
  if (!p_Result.m_Result)
  {
    if (!p_Action.m_MoveDestination.empty())
    {
      SetDialogMessage("Move message failed", true /* p_Warn */);
      LOG_WARNING("move destination = %s", p_Action.m_MoveDestination.c_str());
    }
    else if (p_Action.m_SetSeen || p_Action.m_SetUnseen)
    {
      SetDialogMessage("Update message flags failed", true /* p_Warn */);
    }
    else if (p_Action.m_UploadDraft)
    {
      SetDialogMessage("Saving draft failed, message queued for upload", true /* p_Warn */);
      OfflineQueue::PushDraftMessage(p_Action.m_Msg);
    }
    else if (p_Action.m_UploadMessage)
    {
      SetDialogMessage("Importing message failed", true /* p_Warn */);
    }
    else if (p_Action.m_DeleteMessages)
    {
      SetDialogMessage("Permanently delete message failed", true /* p_Warn */);
    }
    else
    {
      SetDialogMessage("Unknown IMAP action error", true /* p_Warn */);
    }
  }
}

void Ui::SmtpResultHandlerError(const SmtpManager::Result& p_Result)
{
  bool saveDraft = false;
  SmtpManager::Action smtpAction = p_Result.m_Action;
  std::string draftMessage;

  if (!m_DraftsFolder.empty())
  {
    if (smtpAction.m_IsSendCreatedMessage)
    {
      SetDialogMessage("Failed sending queued message, uploading draft");
      saveDraft = true;
      draftMessage = smtpAction.m_CreatedMsg;
    }
    else
    {
      const std::string errStr = Smtp::GetErrorMessage(p_Result.m_SmtpStatus);
      const std::string errMsg = (!errStr.empty())
        ? "Send failed (" + errStr + ")."
        : "Send failed.";
      const std::string msg =
        (smtpAction.m_ComposeDraftUid != 0)
        ? errMsg + " Overwrite draft (y) or queue send (n)?"
        : errMsg + " Save draft (y) or queue send (n)?";
      if (PromptYesNo(msg))
      {
        saveDraft = true;
        smtpAction.m_IsSendMessage = false;
        smtpAction.m_IsCreateMessage = true;

        SmtpManager::Result smtpResult = m_SmtpManager->SyncAction(smtpAction);
        if (smtpResult.m_SmtpStatus == SmtpStatusOk)
        {
          draftMessage = smtpResult.m_Message;
        }
        else
        {
          SetDialogMessage("Message creation failed", true /* p_Warn */);
          return;
        }
      }
    }
  }

  if (saveDraft)
  {
    ImapManager::Action imapAction;
    imapAction.m_UploadDraft = true;
    imapAction.m_Folder = m_DraftsFolder;
    imapAction.m_Msg = draftMessage;
    m_ImapManager->AsyncAction(imapAction);

    if (smtpAction.m_ComposeDraftUid != 0)
    {
      MoveMessages(std::set<uint32_t>({ smtpAction.m_ComposeDraftUid }), m_DraftsFolder,
                   m_TrashFolder);
      m_HasRequestedUids[m_TrashFolder] = false;
    }

    m_HasRequestedUids[m_DraftsFolder] = false;
  }
  else
  {
    if (!smtpAction.m_IsSendCreatedMessage)
    {
      smtpAction.m_IsSendMessage = false;
      smtpAction.m_IsCreateMessage = true;
      SmtpManager::Result smtpResult = m_SmtpManager->SyncAction(smtpAction);
      if (smtpResult.m_SmtpStatus == SmtpStatusOk)
      {
        draftMessage = smtpResult.m_Message;
      }
      else
      {
        SetDialogMessage("Message creation failed", true /* p_Warn */);
      }
    }

    if (!draftMessage.empty())
    {
      OfflineQueue::PushOutboxMessage(draftMessage);
      SetDialogMessage("Message queued for sending");
    }
  }

  AsyncUiRequest(UiRequestDrawAll);
}

void Ui::SmtpResultHandler(const SmtpManager::Result& p_Result)
{
  if (p_Result.m_SmtpStatus != SmtpStatusOk)
  {
    std::unique_lock<std::mutex> lock(m_SmtpErrorMutex);
    m_SmtpErrorResults.push_back(p_Result);
    AsyncUiRequest(UiRequestDrawError);
  }
  else
  {
    const SmtpManager::Action& action = p_Result.m_Action;
    const std::vector<Contact> to = Contact::FromStrings(Util::SplitAddrs(action.m_To));
    const std::vector<Contact> cc = Contact::FromStrings(Util::SplitAddrs(action.m_Cc));
    const std::vector<Contact> bcc = Contact::FromStrings(Util::SplitAddrs(action.m_Bcc));

    std::vector<Contact> contacts;
    std::move(to.begin(), to.end(), std::back_inserter(contacts));
    std::move(cc.begin(), cc.end(), std::back_inserter(contacts));
    std::move(bcc.begin(), bcc.end(), std::back_inserter(contacts));

    for (auto& contact : contacts)
    {
      const std::string& address = contact.GetAddress();

      if (address == m_Address)
      {
        InvalidateUiCache(m_Inbox);
        AsyncUiRequest(UiRequestDrawAll);
        break;
      }
    }

    if ((action.m_ComposeDraftUid != 0) && !m_DraftsFolder.empty() && !m_TrashFolder.empty())
    {
      MoveMessages(std::set<uint32_t>({ action.m_ComposeDraftUid }), m_DraftsFolder,
                   m_TrashFolder);
    }

    if (m_ClientStoreSent)
    {
      if (!m_SentFolder.empty())
      {
        ImapManager::Action imapAction;
        imapAction.m_UploadMessage = true;
        imapAction.m_Folder = m_SentFolder;
        imapAction.m_Msg = p_Result.m_Message;
        m_ImapManager->AsyncAction(imapAction);
      }
      else
      {
        SetDialogMessage("Sent folder not configured", true /* p_Warn */);
      }
    }

    if (!m_SentFolder.empty())
    {
      std::lock_guard<std::mutex> lock(m_Mutex);
      m_HasRequestedUids[m_SentFolder] = false;
    }

    std::string from = action.m_From;
    if (!from.empty())
    {
      AddressBook::AddFrom(from);
    }
    else
    {
      AddressBook::AddFrom(GetDefaultFrom());
    }
  }

  Util::RmDir(p_Result.m_Action.m_ComposeTempDirectory);
}

void Ui::StatusHandler(const StatusUpdate& p_StatusUpdate)
{
  std::lock_guard<std::mutex> lock(m_Mutex);
  m_Status.Update(p_StatusUpdate);

  if (!m_HasRequestedFolders && !m_HasPrefetchRequestedFolders && (m_PrefetchLevel >= PrefetchLevelFullSync) &&
      (p_StatusUpdate.SetFlags & Status::FlagConnected))
  {
    ImapManager::Request request;
    request.m_PrefetchLevel = PrefetchLevelFullSync;
    request.m_GetFolders = true;
    LOG_DEBUG("prefetch req folders");
    m_HasPrefetchRequestedFolders = true;
    m_ImapManager->PrefetchRequest(request);
  }

  char uiRequest = UiRequestDrawAll;
  if (p_StatusUpdate.SetFlags & Status::FlagConnected)
  {
    uiRequest |= UiRequestHandleConnected;
  }

  AsyncUiRequest(uiRequest);
}

void Ui::SearchHandler(const ImapManager::SearchQuery& p_SearchQuery,
                       const ImapManager::SearchResult& p_SearchResult)
{
  {
    std::lock_guard<std::mutex> lock(m_SearchMutex);
    if (p_SearchQuery.m_Offset == 0)
    {
      m_MessageListSearchResultHeaders = p_SearchResult.m_Headers;
      m_MessageListSearchResultFolderUids = p_SearchResult.m_FolderUids;
      LOG_DEBUG("search result offset = %d", p_SearchQuery.m_Offset);
    }
    else if (p_SearchQuery.m_Offset > 0)
    {
      m_MessageListSearchResultHeaders.insert(m_MessageListSearchResultHeaders.end(),
                                              p_SearchResult.m_Headers.begin(), p_SearchResult.m_Headers.end());
      m_MessageListSearchResultFolderUids.insert(m_MessageListSearchResultFolderUids.end(),
                                                 p_SearchResult.m_FolderUids.begin(),
                                                 p_SearchResult.m_FolderUids.end());
      LOG_DEBUG("search result offset = %d", p_SearchQuery.m_Offset);
    }

    m_MessageListSearchHasMore = p_SearchResult.m_HasMore;
  }

  AsyncUiRequest(UiRequestDrawAll);
  UpdateUidFromIndex(false /* p_UserTriggered */);
}

void Ui::SetImapManager(std::shared_ptr<ImapManager> p_ImapManager)
{
  m_ImapManager = p_ImapManager;
  if (m_ImapManager)
  {
    m_ImapManager->SetCurrentFolder(m_CurrentFolder);
  }
}

void Ui::SetSmtpManager(std::shared_ptr<SmtpManager> p_SmtpManager)
{
  m_SmtpManager = p_SmtpManager;
}

void Ui::ResetImapManager()
{
  m_ImapManager.reset();
}

void Ui::ResetSmtpManager()
{
  m_SmtpManager.reset();
}

void Ui::SetTrashFolder(const std::string& p_TrashFolder)
{
  m_TrashFolder = p_TrashFolder;
}

void Ui::SetDraftsFolder(const std::string& p_DraftsFolder)
{
  m_DraftsFolder = p_DraftsFolder;
}

void Ui::SetSentFolder(const std::string& p_SentFolder)
{
  m_SentFolder = p_SentFolder;
}

void Ui::SetClientStoreSent(bool p_ClientStoreSent)
{
  m_ClientStoreSent = p_ClientStoreSent;
}

bool Ui::IsConnected()
{
  std::lock_guard<std::mutex> lock(m_Mutex);
  return m_Status.IsSet(Status::FlagConnected);
}

std::string Ui::GetKeyDisplay(int p_Key)
{
  if (p_Key == '\n')
  {
    return "Re";
  }
  else if ((p_Key >= 0x0) && (p_Key <= 0x1F))
  {
    return "^" + std::string(1, (char)p_Key + 0x40);
  }
  else if (p_Key == ',')
  {
    return "<";
  }
  else if (p_Key == '.')
  {
    return ">";
  }
  else if (p_Key == KEY_LEFT)
  {
    return "←";
  }
  else if (p_Key == KEY_RIGHT)
  {
    return "→";
  }
  else if ((p_Key >= 'a') && (p_Key <= 'z'))
  {
    return std::string(1, std::toupper((char)p_Key));
  }
  else
  {
    return std::string(1, (char)p_Key);
  }

  return "??";
}

std::string Ui::GetStatusStr()
{
  std::lock_guard<std::mutex> lock(m_Mutex);
  return m_Status.ToString();
}

std::string Ui::GetStateStr()
{
  std::lock_guard<std::mutex> lock(m_Mutex);

  switch (m_State)
  {
    case StateViewMessageList:
      if (m_MessageListSearch)
      {
        return "Search: " + m_MessageListSearchQuery;
      }
      else if (m_SortFilter[m_CurrentFolder] != SortDefault)
      {
        return "Folder: " + m_CurrentFolder + GetFilterStateStr();
      }
      else
      {
        return "Folder: " + m_CurrentFolder;
      }
    case StateViewMessage:
      {
        std::string str = std::string("Message ") + (m_Plaintext ? "plain" : "html");
        if (m_MessageViewToggledSeen)
        {
          const std::map<uint32_t, uint32_t>& flags = m_Flags[m_CurrentFolder];
          const int uid = m_MessageListCurrentUid[m_CurrentFolder];
          const bool unread = ((flags.find(uid) != flags.end()) && (!Flag::GetSeen(flags.at(uid))));
          if (unread)
          {
            str += " [unread]";
          }
        }

        return str;
      }
    case StateGotoFolder:
      return "Goto Folder";
    case StateMoveToFolder:
      return "Move To Folder";
    case StateComposeMessage:
      return std::string("Compose") + (m_CurrentMarkdownHtmlCompose ? " Markdown" : "");
    case StateComposeCopyMessage:
      return std::string("Compose Copy") + (m_CurrentMarkdownHtmlCompose ? " Markdown" : "");
    case StateReplyAllMessage:
    case StateReplySenderMessage:
      return std::string("Reply") + (m_CurrentMarkdownHtmlCompose ? " Markdown" : "");
    case StateForwardMessage:
      return std::string("Forward") + (m_CurrentMarkdownHtmlCompose ? " Markdown" : "");
    case StateForwardAttachedMessage:
      return std::string("Forward Attached") + (m_CurrentMarkdownHtmlCompose ? " Markdown" : "");
    case StateAddressList:
    case StateFromAddressList:
      return "Address Book";
    case StateFileList:
      return "File Selection";
    case StateViewPartList:
      return "Message Parts";
    default: return "Unknown State";
  }
}

std::string Ui::GetFilterStateStr()
{
  switch (m_SortFilter[m_CurrentFolder])
  {
    case SortUnseenAsc:
      return " [Unrd Asc]";
    case SortUnseenDesc:
      return " [Unrd Desc]";
    case SortUnseenOnly:
      return " [Unrd Only]";
    case SortAttchAsc:
      return " [Attc Asc]";
    case SortAttchDesc:
      return " [Attc Desc]";
    case SortAttchOnly:
      return " [Attc Only]";
    case SortDateAsc:
      return " [Date Asc]";
    case SortDateDesc:
      return " [Date Desc]";
    case SortCurrDateOnly:
      return " [Date Curr]";
    case SortNameAsc:
      return " [Name Asc]";
    case SortNameDesc:
      return " [Name Desc]";
    case SortCurrNameOnly:
      return " [Name Curr]";
    case SortSubjAsc:
      return " [Subj Asc]";
    case SortSubjDesc:
      return " [Subj Desc]";
    case SortCurrSubjOnly:
      return " [Subj Curr]";
    default:
      return ""; // should not reach here
  }
}

bool Ui::IsValidTextKey(int p_Key)
{
  return ((p_Key >= 0x20) || (p_Key == 0xA));
}

bool Ui::ComposedMessageIsValid(bool p_ForSend)
{
  std::string addrs =
    Util::ToString(GetComposeStr(HeaderTo)) +
    Util::ToString(GetComposeStr(HeaderCc)) +
    Util::ToString(GetComposeStr(HeaderBcc));
  if (p_ForSend && (addrs.find("@") == std::string::npos))
  {
    SetDialogMessage("No recipients specified");
    return false;
  }

  std::vector<std::string> atts = Util::SplitPaths(Util::ToString(GetComposeStr(HeaderAtt)));
  for (auto& att : atts)
  {
    if (!Util::IsReadableFile(att))
    {
      std::string path = att;
      int maxPath = (m_ScreenWidth - 26);
      if ((int)path.size() > maxPath)
      {
        int offset = path.size() - maxPath;
        path = "..." + path.substr(offset);
      }

      LOG_WARNING("file \"%s\" not found", att.c_str());
      SetDialogMessage("File \"" + path + "\" not found");
      return false;
    }
  }

  std::string subject = Util::ToString(GetComposeStr(HeaderSub));
  if (p_ForSend && subject.empty())
  {
    if (!Ui::PromptYesNo("No subject specified, continue (y/n)?"))
    {
      return false;
    }
  }

  return true;
}

void Ui::SendComposedMessage()
{
  SmtpManager::Action smtpAction;
  smtpAction.m_From = Util::ToString(GetComposeStr(HeaderFrom));
  smtpAction.m_To = Util::ToString(GetComposeStr(HeaderTo));
  smtpAction.m_Cc = Util::ToString(GetComposeStr(HeaderCc));
  smtpAction.m_Bcc = Util::ToString(GetComposeStr(HeaderBcc));
  smtpAction.m_Att = Util::ToString(GetComposeStr(HeaderAtt));
  smtpAction.m_Subject = Util::ToString(GetComposeStr(HeaderSub));
  smtpAction.m_Body = Util::ToString(GetComposeBodyForSend());
  smtpAction.m_HtmlBody = MakeHtmlPart(Util::ToString(m_ComposeMessageStr));
  smtpAction.m_RefMsgId = m_ComposeHeaderRef;
  smtpAction.m_ComposeTempDirectory = m_ComposeTempDirectory;
  smtpAction.m_ComposeDraftUid = m_ComposeDraftUid;
  smtpAction.m_FormatFlowed = (m_ComposeLineWrap == LineWrapFormatFlowed);

  if (IsConnected())
  {
    smtpAction.m_IsSendMessage = true;
    m_SmtpManager->AsyncAction(smtpAction);
  }
  else
  {
    smtpAction.m_IsCreateMessage = true;
    SmtpManager::Result smtpResult = m_SmtpManager->SyncAction(smtpAction);
    if (smtpResult.m_SmtpStatus == SmtpStatusOk)
    {
      OfflineQueue::PushOutboxMessage(smtpResult.m_Message);
      SetDialogMessage("Message queued for sending");
    }
    else
    {
      SetDialogMessage("Message creation failed", true /* p_Warn */);
    }
  }
}

void Ui::UploadDraftMessage()
{
  if (!m_DraftsFolder.empty())
  {
    SmtpManager::Action smtpAction;
    smtpAction.m_IsCreateMessage = true;
    smtpAction.m_From = Util::ToString(GetComposeStr(HeaderFrom));
    smtpAction.m_To = Util::ToString(GetComposeStr(HeaderTo));
    smtpAction.m_Cc = Util::ToString(GetComposeStr(HeaderCc));
    smtpAction.m_Bcc = Util::ToString(GetComposeStr(HeaderBcc));
    smtpAction.m_Att = Util::ToString(GetComposeStr(HeaderAtt));
    smtpAction.m_Subject = Util::ToString(GetComposeStr(HeaderSub));
    smtpAction.m_Body = Util::ToString(GetComposeBodyForSend());
    smtpAction.m_HtmlBody = MakeHtmlPart(Util::ToString(m_ComposeMessageStr));
    smtpAction.m_RefMsgId = m_ComposeHeaderRef;

    SmtpManager::Result smtpResult = m_SmtpManager->SyncAction(smtpAction);
    if (smtpResult.m_SmtpStatus == SmtpStatusOk)
    {
      ImapManager::Action imapAction;
      imapAction.m_UploadDraft = true;
      imapAction.m_Folder = m_DraftsFolder;
      imapAction.m_Msg = smtpResult.m_Message;

      if (IsConnected())
      {
        m_ImapManager->AsyncAction(imapAction);

        if (m_ComposeDraftUid != 0)
        {
          MoveMessages(std::set<uint32_t>({ m_ComposeDraftUid }), m_DraftsFolder, m_TrashFolder);
        }

        m_HasRequestedUids[m_DraftsFolder] = false;
      }
      else
      {
        OfflineQueue::PushDraftMessage(smtpResult.m_Message);
        SetDialogMessage("Message queued for draft upload");
      }
    }
    else
    {
      SetDialogMessage("Message creation failed", true /* p_Warn */);
    }
  }
  else
  {
    SetDialogMessage("Drafts folder not configured", true /* p_Warn */);
  }
}

bool Ui::DeleteMessage()
{
  if (!m_TrashFolder.empty())
  {
    const std::string& folder = m_CurrentFolderUid.first;
    bool hasSelection = !m_SelectedUids.empty();
    bool allSelectedItemsInTrash = hasSelection && (m_SelectedUids.size() == 1) &&
      (m_SelectedUids.begin()->first == m_TrashFolder);
    if (allSelectedItemsInTrash || (!hasSelection && (folder == m_TrashFolder)))
    {
      int count = Ui::GetSelectedCount();
      std::string prompt = (count > 1)
        ? "Permanently delete " + std::to_string(count) + " messages (y/n)?"
        : "Permanently delete message (y/n)?";
      if (Ui::PromptYesNo(prompt))
      {
        DeleteSelectedMessages();
        ClearSelection();
      }
    }
    else
    {
      int count = Ui::GetSelectedCount();
      std::string prompt = (count > 1) ? "Delete " + std::to_string(count) + " messages (y/n)?"
                                       : "Delete message (y/n)?";

      if (m_DeleteWithoutConfirm || Ui::PromptYesNo(prompt))
      {
        MoveSelectedMessages(m_TrashFolder);

        if (count > 0)
        {
          ClearSelection();
        }

        m_MessageFindMatchLine = -1;
        m_MessageViewLineOffset = 0;

        bool isHeaderUidsEmpty = false;
        {
          std::lock_guard<std::mutex> lock(m_Mutex);
          isHeaderUidsEmpty = GetHeaderUids(folder).empty();
        }

        if (isHeaderUidsEmpty)
        {
          SetState(StateViewMessageList);
        }
      }
    }

    return true;
  }
  else
  {
    SetDialogMessage("Trash folder not configured", true /* p_Warn */);
    return false;
  }
}

void Ui::MoveSelectedMessages(const std::string& p_To)
{
  int selectCount = 0;
  for (auto& selectedFolder : m_SelectedUids)
  {
    if (!selectedFolder.second.empty())
    {
      MoveMessages(selectedFolder.second, selectedFolder.first, p_To);
    }
    selectCount += selectedFolder.second.size();
  }

  if (selectCount == 0)
  {
    const std::string& folder = m_CurrentFolderUid.first;
    const uint32_t uid = m_CurrentFolderUid.second;
    MoveMessages(std::set<uint32_t>({ uid }), folder, p_To);
  }
}

void Ui::MoveMessages(const std::set<uint32_t>& p_Uids, const std::string& p_From,
                      const std::string& p_To)
{
  ImapManager::Action action;
  action.m_Folder = p_From;
  action.m_Uids = p_Uids;
  action.m_MoveDestination = p_To;
  m_ImapManager->AsyncAction(action);

  const std::string& folder = p_From;

  {
    std::lock_guard<std::mutex> lock(m_Mutex);

    UpdateDisplayUids(folder, action.m_Uids);
    m_Uids[folder] = m_Uids[folder] - action.m_Uids;
    m_Headers[folder] = m_Headers[folder] - action.m_Uids;

    m_HasRequestedUids[p_From] = false;
    m_HasRequestedUids[p_To] = false;
  }

  if (m_MessageListSearch)
  {
    std::lock_guard<std::mutex> lock(m_SearchMutex);
    int resultCount = m_MessageListSearchResultHeaders.size();
    for (int i = 0; i < resultCount; ++i)
    {
      auto& folderUid = m_MessageListSearchResultFolderUids.at(i);
      if ((folderUid.first == folder) && (p_Uids.find(folderUid.second) != p_Uids.end()))
      {
        m_MessageListSearchResultFolderUids.erase(m_MessageListSearchResultFolderUids.begin() + i);
        m_MessageListSearchResultHeaders.erase(m_MessageListSearchResultHeaders.begin() + i);
        --resultCount;
      }
    }
  }

  UpdateIndexFromUid();
}

void Ui::DeleteSelectedMessages()
{
  int selectCount = 0;
  for (auto& selectedFolder : m_SelectedUids)
  {
    if (!selectedFolder.second.empty())
    {
      DeleteMessages(selectedFolder.second, selectedFolder.first);
    }
    selectCount += selectedFolder.second.size();
  }

  if (selectCount == 0)
  {
    const std::string& folder = m_CurrentFolderUid.first;
    const uint32_t uid = m_CurrentFolderUid.second;
    DeleteMessages(std::set<uint32_t>({ uid }), folder);
  }
}

void Ui::DeleteMessages(const std::set<uint32_t>& p_Uids, const std::string& p_Folder)
{
  ImapManager::Action action;
  action.m_Folder = p_Folder;
  action.m_Uids = p_Uids;
  action.m_DeleteMessages = true;
  m_ImapManager->AsyncAction(action);

  {
    std::lock_guard<std::mutex> lock(m_Mutex);
    UpdateDisplayUids(p_Folder, action.m_Uids);
    m_Uids[p_Folder] = m_Uids[p_Folder] - action.m_Uids;
    m_Headers[p_Folder] = m_Headers[p_Folder] - action.m_Uids;

    m_HasRequestedUids[p_Folder] = false;
  }

  if (m_MessageListSearch)
  {
    std::lock_guard<std::mutex> lock(m_SearchMutex);
    int resultCount = m_MessageListSearchResultHeaders.size();
    for (int i = 0; i < resultCount; ++i)
    {
      auto& folderUid = m_MessageListSearchResultFolderUids.at(i);
      if ((folderUid.first == p_Folder) && (p_Uids.find(folderUid.second) != p_Uids.end()))
      {
        m_MessageListSearchResultFolderUids.erase(m_MessageListSearchResultFolderUids.begin() + i);
        m_MessageListSearchResultHeaders.erase(m_MessageListSearchResultHeaders.begin() + i);
        --resultCount;
      }
    }
  }

  UpdateIndexFromUid();
}

void Ui::ToggleSeen()
{
  if (m_SelectedUids.empty())
  {
    const std::string& folder = m_CurrentFolderUid.first;
    const int uid = m_CurrentFolderUid.second;
    std::map<uint32_t, uint32_t> flags;
    {
      std::lock_guard<std::mutex> lock(m_Mutex);
      flags = m_Flags[folder];
    }
    bool oldSeen = ((flags.find(uid) != flags.end()) && (Flag::GetSeen(flags.at(uid))));
    bool newSeen = !oldSeen;
    std::set<uint32_t> uids;
    uids.insert(uid);

    SetSeen(folder, uids, newSeen);
  }
  else
  {
    bool newSeen = true;
    bool newSeenSet = false;
    for (auto& selectedFolder : m_SelectedUids)
    {
      if (!selectedFolder.second.empty())
      {
        if (!newSeenSet)
        {
          std::map<uint32_t, uint32_t> flags;
          {
            std::lock_guard<std::mutex> lock(m_Mutex);
            flags = m_Flags[selectedFolder.first];
          }

          uint32_t firstUid = *selectedFolder.second.rbegin();
          bool oldSeen = ((flags.find(firstUid) != flags.end()) && (Flag::GetSeen(flags.at(firstUid))));
          newSeen = !oldSeen;
          newSeenSet = true;
        }

        SetSeen(selectedFolder.first, selectedFolder.second, newSeen);
      }
    }
  }
}

void Ui::SetSeen(const std::string& p_Folder, const std::set<uint32_t>& p_Uids, bool p_Seen)
{
  ImapManager::Action action;
  action.m_Folder = p_Folder;
  action.m_Uids = p_Uids;
  action.m_SetSeen = p_Seen;
  action.m_SetUnseen = !p_Seen;
  m_ImapManager->AsyncAction(action);

  {
    std::lock_guard<std::mutex> lock(m_Mutex);
    for (auto& uid : p_Uids)
    {
      Flag::SetSeen(m_Flags[p_Folder][uid], p_Seen);
    }
  }
}

void Ui::MarkSeen()
{
  std::map<uint32_t, uint32_t> flags;
  const std::string& folder = m_CurrentFolderUid.first;
  const int uid = m_CurrentFolderUid.second;

  {
    std::lock_guard<std::mutex> lock(m_Mutex);
    flags = m_Flags[folder];
  }

  bool oldSeen = ((flags.find(uid) != flags.end()) && (Flag::GetSeen(flags.at(uid))));

  if (oldSeen) return;

  bool newSeen = true;

  ImapManager::Action action;
  action.m_Folder = folder;
  action.m_Uids.insert(uid);
  action.m_SetSeen = newSeen;
  action.m_SetUnseen = !newSeen;
  m_ImapManager->AsyncAction(action);

  {
    std::lock_guard<std::mutex> lock(m_Mutex);
    Flag::SetSeen(m_Flags[folder][uid], newSeen);
  }
}

void Ui::UpdateUidFromIndex(bool p_UserTriggered)
{
  if (m_MessageListSearch)
  {
    std::lock_guard<std::mutex> lock(m_SearchMutex);
    const std::vector<Header>& headers = m_MessageListSearchResultHeaders;
    m_MessageListCurrentIndex[m_CurrentFolder] =
      Util::Bound(0, m_MessageListCurrentIndex[m_CurrentFolder], (int)headers.size() - 1);

    const int32_t idx = m_MessageListCurrentIndex[m_CurrentFolder];
    if (idx < (int)m_MessageListSearchResultFolderUids.size())
    {
      m_CurrentFolderUid.first = m_MessageListSearchResultFolderUids[idx].first;
      m_CurrentFolderUid.second = m_MessageListSearchResultFolderUids[idx].second;
    }

    if (m_MessageListSearchHasMore &&
        ((m_MessageListCurrentIndex[m_CurrentFolder] + m_MainWinHeight) >= ((int)headers.size())))
    {
      m_MessageListSearchOffset += m_MessageListSearchMax;
      m_MessageListSearchMax = m_MainWinHeight;
      m_MessageListSearchHasMore = false;

      ImapManager::SearchQuery searchQuery;
      searchQuery.m_QueryStr = m_MessageListSearchQuery;
      searchQuery.m_Offset = m_MessageListSearchOffset;
      searchQuery.m_Max = m_MessageListSearchMax;

      LOG_DEBUG("search str = \"%s\" offset = %d max = %d",
                searchQuery.m_QueryStr.c_str(), searchQuery.m_Offset, searchQuery.m_Max);
      m_ImapManager->AsyncSearch(searchQuery);
    }

    return;
  }

  std::lock_guard<std::mutex> lock(m_Mutex);
  const std::map<std::string, uint32_t>& displayUids = GetDisplayUids(m_CurrentFolder);

  m_MessageListCurrentIndex[m_CurrentFolder] =
    Util::Bound(0, m_MessageListCurrentIndex[m_CurrentFolder], (int)displayUids.size() - 1);
  if (displayUids.size() > 0)
  {
    m_MessageListCurrentUid[m_CurrentFolder] =
      std::prev(displayUids.end(), m_MessageListCurrentIndex[m_CurrentFolder] + 1)->second;
  }
  else
  {
    m_MessageListCurrentUid[m_CurrentFolder] = -1;
  }

  m_CurrentFolderUid.first = m_CurrentFolder;
  m_CurrentFolderUid.second = m_MessageListCurrentUid[m_CurrentFolder];

  m_MessageListUidSet[m_CurrentFolder] = p_UserTriggered;

  static int lastUid = 0;
  if (lastUid != m_MessageListCurrentUid[m_CurrentFolder])
  {
    m_MessageViewToggledSeen = false;
    lastUid = m_MessageListCurrentUid[m_CurrentFolder];
  }

  LOG_TRACE("current uid = %d, idx = %d", m_MessageListCurrentUid[m_CurrentFolder],
            m_MessageListCurrentIndex[m_CurrentFolder]);
}

void Ui::UpdateIndexFromUid()
{
  if (m_MessageListSearch) return;

  bool found = false;

  {
    std::lock_guard<std::mutex> lock(m_Mutex);

    if (m_MessageListUidSet[m_CurrentFolder])
    {
      const std::map<std::string, uint32_t>& displayUids = GetDisplayUids(m_CurrentFolder);

      for (auto it = displayUids.rbegin(); it != displayUids.rend(); ++it)
      {
        if ((int32_t)it->second == m_MessageListCurrentUid[m_CurrentFolder])
        {
          m_MessageListCurrentIndex[m_CurrentFolder] = std::distance(displayUids.rbegin(), it);
          found = true;
          break;
        }
      }
    }
  }

  if (!found)
  {
    UpdateUidFromIndex(false /* p_UserTriggered */);
  }
  else
  {
    m_CurrentFolderUid.first = m_CurrentFolder;
    m_CurrentFolderUid.second = m_MessageListCurrentUid[m_CurrentFolder];
  }

  LOG_TRACE("current uid = %d, idx = %d", m_MessageListCurrentUid[m_CurrentFolder],
            m_MessageListCurrentIndex[m_CurrentFolder]);
}

void Ui::ComposeMessagePrevLine()
{
  if (m_ComposeMessageWrapLine > 0)
  {
    int stepsBack = 0;
    if ((int)m_ComposeMessageLines[m_ComposeMessageWrapLine - 1].size() >
        m_ComposeMessageWrapPos)
    {
      stepsBack = m_ComposeMessageLines[m_ComposeMessageWrapLine - 1].size() + 1;
    }
    else
    {
      stepsBack = m_ComposeMessageWrapPos + 1;
    }

    stepsBack = std::min(stepsBack, m_MaxComposeLineLength);
    m_ComposeMessagePos = Util::Bound(0, m_ComposeMessagePos - stepsBack,
                                      (int)m_ComposeMessageStr.size());
  }
  else
  {
    m_IsComposeHeader = true;
  }
}

void Ui::ComposeMessageNextLine()
{
  if (m_ComposeMessagePos < (int)m_ComposeMessageStr.size())
  {
    int stepsForward = (int)m_ComposeMessageLines[m_ComposeMessageWrapLine].size() -
      m_ComposeMessageWrapPos + 1;
    if ((m_ComposeMessageWrapLine + 1) < (int)m_ComposeMessageLines.size())
    {
      if ((int)m_ComposeMessageLines[m_ComposeMessageWrapLine + 1].size() >
          m_ComposeMessageWrapPos)
      {
        stepsForward += m_ComposeMessageWrapPos;
      }
      else
      {
        stepsForward += (int)m_ComposeMessageLines[m_ComposeMessageWrapLine + 1].size();
      }
    }

    stepsForward = std::min(stepsForward, m_MaxComposeLineLength);
    m_ComposeMessagePos = Util::Bound(0, m_ComposeMessagePos + stepsForward,
                                      (int)m_ComposeMessageStr.size());
  }
}

int Ui::ReadKeyBlocking()
{
  while (true)
  {
    fd_set fds;
    FD_ZERO(&fds);
    FD_SET(STDIN_FILENO, &fds);
    int maxfd = STDIN_FILENO;
    struct timeval tv = {1, 0};
    int rv = select(maxfd + 1, &fds, NULL, NULL, &tv);

    if (rv == 0) continue;

    if (FD_ISSET(STDIN_FILENO, &fds))
    {
      wint_t key = 0;
      get_wch(&key);

      return key;
    }
  }
}

bool Ui::PromptYesNo(const std::string& p_Prompt)
{
  werase(m_DialogWin);

  const std::string& dispStr = p_Prompt;
  int x = std::max((m_ScreenWidth - (int)dispStr.size() - 1) / 2, 0);
  wattron(m_DialogWin, m_AttrsDialog);
  mvwprintw(m_DialogWin, 0, x, " %s ", dispStr.c_str());
  wattroff(m_DialogWin, m_AttrsDialog);

  wrefresh(m_DialogWin);

  int key = ReadKeyBlocking();

  return ((key == 'y') || (key == 'Y'));
}

bool Ui::PromptString(const std::string& p_Prompt, const std::string& p_Action,
                      std::string& p_Entry)
{
  if (m_HelpEnabled)
  {
    werase(m_HelpWin);
    static std::vector<std::vector<std::string>> savePartHelp =
    {
      {
        GetKeyDisplay(KEY_RETURN), p_Action,
      },
      {
        GetKeyDisplay(m_KeyCancel), "Cancel",
      }
    };

    DrawHelpText(savePartHelp);

    wrefresh(m_HelpWin);
  }

  curs_set(1);

  m_FilenameEntryString = Util::ToWString(p_Entry);
  m_FilenameEntryStringPos = m_FilenameEntryString.size();

  bool rv = false;
  while (true)
  {
    werase(m_DialogWin);

    const std::string& dispStr = p_Prompt + Util::ToString(m_FilenameEntryString);
    mvwprintw(m_DialogWin, 0, 3, "%s", dispStr.c_str());

    leaveok(m_DialogWin, false);
    wmove(m_DialogWin, 0, 3 + p_Prompt.size() + m_FilenameEntryStringPos);
    wrefresh(m_DialogWin);
    leaveok(m_DialogWin, true);

    int key = ReadKeyBlocking();
    if (key == m_KeyCancel)
    {
      rv = false;
      break;
    }
    else if ((key == KEY_RETURN) || (key == KEY_ENTER))
    {
      p_Entry = Util::ToString(m_FilenameEntryString);
      rv = true;
      break;
    }
    else if ((key == KEY_UP) || (key == KEY_DOWN) ||
             (key == m_KeyPrevPageCompose) || (key == m_KeyNextPageCompose) ||
             (key == KEY_HOME) || (key == KEY_END))
    {
      // ignore
    }
    else if (HandleLineKey(key, m_FilenameEntryString, m_FilenameEntryStringPos))
    {
      // none
    }
    else if (HandleTextKey(key, m_FilenameEntryString, m_FilenameEntryStringPos))
    {
      // none
    }
  }

  curs_set(0);
  return rv;
}

bool Ui::CurrentMessageBodyHeaderAvailable()
{
  std::lock_guard<std::mutex> lock(m_Mutex);
  const std::string& folder = m_CurrentFolderUid.first;
  const int uid = m_CurrentFolderUid.second;
  const std::map<uint32_t, Body>& bodys = m_Bodys[folder];
  std::map<uint32_t, Header>& headers = m_Headers[folder];
  std::map<uint32_t, Body>::const_iterator bit = bodys.find(uid);
  std::map<uint32_t, Header>::iterator hit = headers.find(uid);
  return ((hit != headers.end()) && (bit != bodys.end()));
}

void Ui::InvalidateUiCache(const std::string& p_Folder)
{
  std::lock_guard<std::mutex> lock(m_Mutex);
  m_HasRequestedUids[p_Folder] = false;
  m_Flags[p_Folder].clear();
  m_RequestedFlags[p_Folder].clear();
}

void Ui::ExtEditor(const std::string& p_EditorCmd, std::wstring& p_ComposeMessageStr, int& p_ComposeMessagePos)
{
  endwin();
  const std::string& tempPath = Util::GetTempFilename(".txt");
  Util::WriteWFile(tempPath, p_ComposeMessageStr);
  const std::string& cmd = p_EditorCmd + " " + tempPath;
  LOG_DEBUG("launching external editor: %s", cmd.c_str());
  int rv = system(cmd.c_str());
  if (rv == 0)
  {
    LOG_DEBUG("external editor exited successfully");
    p_ComposeMessageStr = Util::ReadWFile(tempPath);
    p_ComposeMessagePos = 0;
  }
  else
  {
    LOG_WARNING("external editor exited with %d", rv);
    Util::DetectCommandNotPresent(cmd);
  }

  Util::DeleteFile(tempPath);

  refresh();

  wint_t key = 0;
  while (get_wch(&key) != ERR)
  {
    // Discard any remaining input
  }
}

void Ui::ExtPager()
{
  endwin();
  const std::string& tempPath = Util::GetTempFilename(".txt");
  Util::WriteFile(tempPath, m_CurrentMessageViewText);
  const std::string& pager = Util::GetPagerCmd();
  const std::string& cmd = pager + " " + tempPath;
  LOG_DEBUG("launching external pager: %s", cmd.c_str());
  int rv = system(cmd.c_str());
  if (rv == 0)
  {
    LOG_DEBUG("external pager exited successfully");
  }
  else
  {
    LOG_WARNING("external pager exited with %d", rv);
    Util::DetectCommandNotPresent(cmd);
  }

  Util::DeleteFile(tempPath);

  refresh();

  wint_t key = 0;
  while (get_wch(&key) != ERR)
  {
    // Discard any remaining input
  }
}

int Ui::ExtPartsViewer(const std::string& p_Path)
{
  const bool isDefaultPartsViewerCmd = Util::IsDefaultPartsViewerCmd();
  if (!isDefaultPartsViewerCmd)
  {
    endwin();
  }

  const std::string& viewer = Util::GetPartsViewerCmd();

  std::string escapedPath = p_Path;
  Util::ReplaceString(escapedPath, "\"", "\\\"");
  const std::string& cmd = viewer + " \"" + escapedPath + "\"";
  LOG_DEBUG("launching external viewer: %s", cmd.c_str());
  int rv = system(cmd.c_str());
  if (rv == 0)
  {
    LOG_DEBUG("external viewer exited successfully");
  }
  else
  {
    LOG_WARNING("external viewer exited with %d", rv);
    Util::DetectCommandNotPresent(cmd);
  }

  if (!isDefaultPartsViewerCmd)
  {
    refresh();

    wint_t key = 0;
    while (get_wch(&key) != ERR)
    {
      // Discard any remaining input
    }
  }

  return rv;
}

void Ui::ExtHtmlViewer()
{
  static const std::string& tempPath = Util::GetTempDir() + std::string("htmlview/tmp.html");
  Util::DeleteFile(tempPath);

  {
    std::lock_guard<std::mutex> lock(m_Mutex);
    const std::string& folder = m_CurrentFolderUid.first;
    const int uid = m_CurrentFolderUid.second;
    std::map<uint32_t, Body>& bodys = m_Bodys[folder];
    std::map<uint32_t, Body>::iterator bodyIt = bodys.find(uid);
    if (bodyIt != bodys.end())
    {
      Body& body = bodyIt->second;
      const std::string& html = body.GetHtml(); // falls back to text/plain if no html
      Util::WriteFile(tempPath, html);
    }
  }

  if (Util::Exists(tempPath))
  {
    int rv = ExtHtmlViewer(tempPath);
    if (rv == 0)
    {
      MarkSeen();
    }
  }
  else
  {
    SetDialogMessage("View html failed (message not available)", true /* p_Warn */);
  }
}

int Ui::ExtHtmlViewer(const std::string& p_Path)
{
  const bool isDefaultHtmlViewerCmd = Util::IsDefaultHtmlViewerCmd();
  if (!isDefaultHtmlViewerCmd)
  {
    endwin();
  }

  const std::string& viewer = Util::GetHtmlViewerCmd();
  const std::string& cmd = viewer + " \"" + p_Path + "\"";
  LOG_DEBUG("launching html viewer: %s", cmd.c_str());
  int rv = system(cmd.c_str());
  if (rv == 0)
  {
    LOG_DEBUG("html viewer exited successfully");
  }
  else
  {
    LOG_WARNING("html viewer exited with %d", rv);
    Util::DetectCommandNotPresent(cmd);
  }

  if (!isDefaultHtmlViewerCmd)
  {
    refresh();

    wint_t key = 0;
    while (get_wch(&key) != ERR)
    {
      // Discard any remaining input
    }
  }

  return rv;
}

int Ui::ExtHtmlPreview(const std::string& p_Path)
{
  const bool isDefaultHtmlPreviewCmd = Util::IsDefaultHtmlPreviewCmd();
  if (!isDefaultHtmlPreviewCmd)
  {
    endwin();
  }

  const std::string& viewer = Util::GetHtmlPreviewCmd();
  const std::string& cmd = viewer + " \"" + p_Path + "\"";
  LOG_DEBUG("launching html viewer: %s", cmd.c_str());
  int rv = system(cmd.c_str());
  if (rv == 0)
  {
    LOG_DEBUG("html viewer exited successfully");
  }
  else
  {
    LOG_WARNING("html viewer exited with %d", rv);
    Util::DetectCommandNotPresent(cmd);
  }

  if (!isDefaultHtmlPreviewCmd)
  {
    refresh();

    wint_t key = 0;
    while (get_wch(&key) != ERR)
    {
      // Discard any remaining input
    }
  }

  return rv;
}

void Ui::ExtMsgViewer()
{
  static const std::string& tempPath = Util::GetTempDir() + std::string("msgview/tmp.eml");
  Util::DeleteFile(tempPath);

  {
    std::lock_guard<std::mutex> lock(m_Mutex);
    const std::string& folder = m_CurrentFolderUid.first;
    const int uid = m_CurrentFolderUid.second;
    std::map<uint32_t, Body>& bodys = m_Bodys[folder];
    std::map<uint32_t, Body>::iterator bodyIt = bodys.find(uid);
    if (bodyIt != bodys.end())
    {
      Body& body = bodyIt->second;
      const std::string& data = body.GetData(); // falls back to text/plain if no html
      Util::WriteFile(tempPath, data);
    }
  }

  if (Util::Exists(tempPath))
  {
    int rv = ExtMsgViewer(tempPath);
    if (rv == 0)
    {
      MarkSeen();
    }
  }
  else
  {
    SetDialogMessage("View message failed (message not available)", true /* p_Warn */);
  }
}

int Ui::ExtMsgViewer(const std::string& p_Path)
{
  const bool isDefaultMsgViewerCmd = Util::IsDefaultMsgViewerCmd();
  if (!isDefaultMsgViewerCmd)
  {
    endwin();
  }

  const std::string& viewer = Util::GetMsgViewerCmd();
  const std::string& cmd = viewer + " \"" + p_Path + "\"";
  LOG_DEBUG("launching message viewer: %s", cmd.c_str());
  int rv = system(cmd.c_str());
  if (rv == 0)
  {
    LOG_DEBUG("message viewer exited successfully");
  }
  else
  {
    LOG_WARNING("message viewer exited with %d", rv);
    Util::DetectCommandNotPresent(cmd);
  }

  if (!isDefaultMsgViewerCmd)
  {
    refresh();

    wint_t key = 0;
    while (get_wch(&key) != ERR)
    {
      // Discard any remaining input
    }
  }

  return rv;
}

void Ui::SetLastStateOrMessageList()
{
  bool isHeaderUidsEmpty = false;
  if (m_MessageListSearch)
  {
    isHeaderUidsEmpty = false;
  }
  else
  {
    std::lock_guard<std::mutex> lock(m_Mutex);
    isHeaderUidsEmpty = GetHeaderUids(m_CurrentFolder).empty();
  }

  if (isHeaderUidsEmpty)
  {
    SetState(StateViewMessageList);
  }
  else
  {
    SetState(m_LastState);
  }

  UpdateIndexFromUid();
}

void Ui::ExportMessage()
{
  const std::string& folder = m_CurrentFolderUid.first;
  const int uid = m_CurrentFolderUid.second;
  std::string filename = Util::GetDownloadsDir() + std::to_string(uid) + ".eml";
  if (PromptString("Export Filename: ", "Save", filename))
  {
    if (!filename.empty())
    {
      filename = Util::ExpandPath(filename);
      std::unique_lock<std::mutex> lock(m_Mutex);
      const std::map<uint32_t, Body>& bodys = m_Bodys[folder];
      if (bodys.find(uid) != bodys.end())
      {
        Util::WriteFile(filename, m_Bodys[folder][uid].GetData());
        lock.unlock();
        SetDialogMessage("Message exported");
      }
      else
      {
        lock.unlock();
        SetDialogMessage("Export failed (message not available)", true /* p_Warn */);
      }
    }
    else
    {
      SetDialogMessage("Export cancelled (empty filename)");
    }
  }
  else
  {
    SetDialogMessage("Export cancelled");
  }
}

void Ui::ImportMessage()
{
  std::string filename = Util::GetDownloadsDir();
  if (PromptString("Import Filename: ", "Load", filename))
  {
    if (!filename.empty())
    {
      filename = Util::ExpandPath(filename);
      if (Util::NotEmpty(filename))
      {
        const std::string& msg = Util::ReadFile(filename);

        ImapManager::Action imapAction;
        imapAction.m_UploadMessage = true;
        imapAction.m_Folder = m_CurrentFolder;
        imapAction.m_Msg = msg;
        m_ImapManager->AsyncAction(imapAction);
        m_HasRequestedUids[m_CurrentFolder] = false;
      }
      else
      {
        SetDialogMessage("Import failed (file not found or empty)");
      }
    }
    else
    {
      SetDialogMessage("Import cancelled (empty filename)");
    }
  }
  else
  {
    SetDialogMessage("Import cancelled");
  }
}

void Ui::SearchMessageBasedOnCurrent(bool p_Subject)
{
  std::string current;
  bool found = false;

  if (m_MessageListSearch)
  {
    std::lock_guard<std::mutex> lock(m_SearchMutex);
    std::vector<Header>& headers = m_MessageListSearchResultHeaders;
    int idx = m_MessageListCurrentIndex[m_CurrentFolder];
    if ((idx >= 0) && (idx < (int)headers.size()))
    {
      current = p_Subject ? headers[idx].GetSubject()
                          : headers[idx].GetShortFrom();
      found = true;
    }
  }
  else
  {
    std::lock_guard<std::mutex> lock(m_Mutex);
    const std::string& folder = m_CurrentFolderUid.first;
    const int uid = m_CurrentFolderUid.second;
    std::map<uint32_t, Header>& headers = m_Headers[folder];
    std::map<uint32_t, Header>::iterator hit = headers.find(uid);
    if (hit != headers.end())
    {
      current = p_Subject ? hit->second.GetSubject()
                          : ((folder != m_SentFolder) ? hit->second.GetShortFrom() : hit->second.GetShortTo());
      found = true;
    }
  }

  if (found)
  {
    current = Util::Trim(current);
    if (p_Subject)
    {
      Util::NormalizeSubject(current, true /*p_ToLower*/);
    }
    else
    {
      Util::NormalizeName(current);
    }
    std::string query = p_Subject ? ("subject:\"" + current + "\"")
                                  : ("from:\"" + current + "\"");
    SearchMessage(query);
  }
  else
  {
    SetDialogMessage("No message selected to search based on");
  }
}

void Ui::SearchMessage(const std::string& p_Query /*= std::string()*/)
{
  std::string query = !p_Query.empty()
    ? p_Query
    : ((m_MessageListSearch && m_PersistSearchQuery) ? m_MessageListSearchQuery : "");
  if (!p_Query.empty() || PromptString("Search Emails: ", "Search", query))
  {
    if (!query.empty())
    {
      m_MessageListSearch = true;
      if (m_CurrentFolder != "")
      {
        m_PreviousFolder = m_CurrentFolder;
        m_CurrentFolder = "";
      }

      m_MessageListCurrentIndex[m_CurrentFolder] = 0;
      ClearSelection();

      {
        std::lock_guard<std::mutex> lock(m_SearchMutex);
        m_MessageListSearchQuery = query;
        m_MessageListSearchOffset = 0;
        m_MessageListSearchMax = m_MainWinHeight + m_MainWinHeight;
        m_MessageListSearchHasMore = false;
        m_MessageListSearchResultHeaders.clear();
        m_MessageListSearchResultFolderUids.clear();
      }

      ImapManager::SearchQuery searchQuery;
      searchQuery.m_QueryStr = query;
      searchQuery.m_Offset = 0;
      searchQuery.m_Max = 2 * m_MainWinHeight;

      LOG_DEBUG("search str=\"%s\" offset=%d max=%d",
                searchQuery.m_QueryStr.c_str(), searchQuery.m_Offset, searchQuery.m_Max);
      m_ImapManager->AsyncSearch(searchQuery);
    }
    else
    {
      m_MessageListSearch = false;
      ClearSelection();
      if (m_PreviousFolder != "")
      {
        m_CurrentFolder = m_PreviousFolder;
        m_PreviousFolder = "";
      }

      UpdateIndexFromUid();
    }
  }
}

void Ui::MessageFind()
{
  std::string query = m_PersistFindQuery ? m_MessageFindQuery : "";
  if (PromptString("Find Text: ", "Find", query))
  {
    if (!query.empty() && (query != m_MessageFindQuery))
    {
      m_MessageFindMatchLine = -1;
      m_MessageFindQuery = query;
    }

    if (!m_MessageFindQuery.empty())
    {
      MessageFindNext();
    }
  }
}

void Ui::MessageFindNext()
{
  int findFromLine = m_MessageFindMatchLine + 1;
  const std::wstring wquery = Util::ToLower(Util::ToWString(m_MessageFindQuery));;
  std::vector<std::wstring> wlines = Ui::GetCachedWordWrapLines("", 0);
  int countLines = wlines.size();

  bool found = false;
  for (int i = findFromLine; i < countLines; ++i)
  {
    std::wstring wline = Util::ToLower(wlines.at(i));
    size_t pos = wline.find(wquery);
    if (pos != std::string::npos)
    {
      m_MessageViewLineOffset = Util::Bound(0, i, countLines - m_MainWinHeight);
      m_MessageFindMatchLine = i;
      m_MessageFindMatchPos = pos;
      found = true;
      break;
    }
  }

  if (!found)
  {
    SetDialogMessage((m_MessageFindMatchLine == -1) ? "No matches found" : "No more matches found");
    m_MessageFindMatchLine = -1; // wrap around search
  }
  else
  {
    SetDialogMessage("");
  }
}

void Ui::Quit()
{
  if (m_QuitWithoutConfirm || Ui::PromptYesNo("Quit falaclient (y/n)?"))
  {
    StatusUpdate statusUpdate;
    statusUpdate.SetFlags = m_Status.IsSet(Status::FlagConnected) ? Status::FlagDisconnecting
                                                                  : Status::FlagExiting;
    m_Status.Update(statusUpdate);
    SetRunning(false);
    LOG_DEBUG("stop thread");
  }
}

std::wstring Ui::GetComposeStr(int p_HeaderField)
{
  if (m_ShowRichHeader)
  {
    return m_ComposeHeaderStr[p_HeaderField];
  }
  else
  {
    switch (p_HeaderField)
    {
      case HeaderTo: return m_ComposeHeaderStr[0];

      case HeaderCc: return m_ComposeHeaderStr[1];

      case HeaderAtt: return m_ComposeHeaderStr[2];

      case HeaderSub: return m_ComposeHeaderStr[3];

      default: break;
    }
  }

  return L"";
}

void Ui::SetComposeStr(int p_HeaderField, const std::wstring& p_Str)
{
  if (p_HeaderField == HeaderAll)
  {
    m_ComposeHeaderStr.clear();
    m_ComposeHeaderStr[0] = p_Str;
    m_ComposeHeaderStr[1] = p_Str;
    m_ComposeHeaderStr[2] = p_Str;
    m_ComposeHeaderStr[3] = p_Str;
    if (m_ShowRichHeader)
    {
      m_ComposeHeaderStr[4] = p_Str;
      m_ComposeHeaderStr[5] = p_Str;
    }
  }
  else
  {
    if (m_ShowRichHeader)
    {
      m_ComposeHeaderStr[p_HeaderField] = p_Str;
    }
    else
    {
      switch (p_HeaderField)
      {
        case HeaderTo:
          m_ComposeHeaderStr[0] = p_Str;
          break;

        case HeaderCc:
          m_ComposeHeaderStr[1] = p_Str;
          break;

        case HeaderAtt:
          m_ComposeHeaderStr[2] = p_Str;
          break;

        case HeaderSub:
          m_ComposeHeaderStr[3] = p_Str;
          break;

        default:
          break;
      }
    }
  }
}

std::wstring Ui::GetComposeBodyForSend()
{
  if (m_ComposeLineWrap == LineWrapNone)
  {
    return m_ComposeMessageStr;
  }
  else if (m_ComposeLineWrap == LineWrapFormatFlowed)
  {
    const bool processFlowed = m_CurrentMessageProcessFlowed;
    const bool outputFlowed = true;
    const bool quoteWrap = m_RewrapQuotedLines;
    const int expandTabSize = 0; // disabled
    std::vector<std::wstring> indentBodyLines =
      Util::WordWrap(m_ComposeMessageStr, 72, processFlowed, outputFlowed, quoteWrap, expandTabSize);
    return Util::Join(indentBodyLines);
  }
  else if (m_ComposeLineWrap == LineWrapHardWrap)
  {
    return Util::Join(m_ComposeMessageLines);
  }
  else
  {
    LOG_WARNING("invalid line wrap %d", m_ComposeLineWrap);
  }
  return L"";
}

int Ui::GetCurrentHeaderField()
{
  if (m_ShowRichHeader)
  {
    switch (m_ComposeHeaderLine)
    {
      case 0: return HeaderFrom;

      case 1: return HeaderTo;

      case 2: return HeaderCc;

      case 3: return HeaderBcc;

      case 4: return HeaderAtt;

      case 5: return HeaderSub;

      default: break;
    }
  }
  else
  {
    switch (m_ComposeHeaderLine)
    {
      case 0: return HeaderTo;

      case 1: return HeaderCc;

      case 2: return HeaderAtt;

      case 3: return HeaderSub;

      default: break;
    }
  }

  return HeaderAll;
}

void Ui::StartSync()
{
  if (IsConnected())
  {
    if (m_PrefetchLevel < PrefetchLevelFullSync)
    {
      LOG_DEBUG("manual full sync started");
      m_PrefetchLevel = PrefetchLevelFullSync;

      ImapManager::Request request;
      request.m_PrefetchLevel = PrefetchLevelFullSync;
      request.m_GetFolders = true;
      LOG_DEBUG("prefetch req folders");
      m_HasPrefetchRequestedFolders = true;
      m_ImapManager->PrefetchRequest(request);
    }
    else
    {
      SetDialogMessage("Sync already enabled", true /* p_Warn */);
    }
  }
  else
  {
    SetDialogMessage("Cannot sync while offline", true /* p_Warn */);
  }
}

std::string Ui::MakeHtmlPart(const std::string& p_Text)
{
  if (!m_CurrentMarkdownHtmlCompose) return std::string();

  if (m_Signature)
  {
    std::string htmlPartCustomSignature = MakeHtmlPartCustomSig(p_Text);
    if (!htmlPartCustomSignature.empty())
    {
      return htmlPartCustomSignature;
    }
  }

  return Util::ConvertTextToHtml(p_Text);
}

std::string Ui::MakeHtmlPartCustomSig(const std::string& p_Text)
{
  std::string signatureStr = Util::ToString(GetSignatureStr(true /* p_NoPrefix */));
  static const std::string sigHtmlPath = Util::GetApplicationDir() + "signature.html";
  if (!Util::IsReadableFile(sigHtmlPath))
  {
    return std::string();
  }

  std::string signatureHtmlStr = Util::ReadFile(sigHtmlPath);
  if (signatureHtmlStr.empty())
  {
    LOG_WARNING("html signature file is empty");
    return std::string();
  }

  char randhex[80];
  snprintf(randhex, sizeof(randhex), "%lx.%lx.%x.%lx",
           (long)time(NULL), random(), getpid(), random());
  const std::string placeholderStr = std::string(randhex);

  std::string tmpText = p_Text;
  if (!Util::ReplaceStringFirst(tmpText, signatureStr, "  \n" + placeholderStr))
  {
    LOG_WARNING("plain text signature not found in message, cannot use custom html signature");
    return std::string();
  }

  tmpText = Util::ConvertTextToHtml(tmpText);
  const std::string placeholderHtmlStr = "<p>" + placeholderStr + "</p>";
  if (Util::ReplaceStringCount(tmpText, placeholderHtmlStr, signatureHtmlStr) != 1)
  {
    LOG_WARNING("unique custom html signature placeholder not found");
    return std::string();
  }

  return tmpText;
}

void Ui::SetRunning(bool p_Running)
{
  s_Running = p_Running;
}

void Ui::HandleConnected()
{
  if (IsConnected())
  {
    std::vector<std::string> draftMsgs = OfflineQueue::PopDraftMessages();
    for (const auto& draftMsg : draftMsgs)
    {
      SetDialogMessage("Uploading queued draft messages");

      ImapManager::Action imapAction;
      imapAction.m_UploadDraft = true;
      imapAction.m_Folder = m_DraftsFolder;
      imapAction.m_Msg = draftMsg;
      m_ImapManager->AsyncAction(imapAction);

      m_HasRequestedUids[m_DraftsFolder] = false;
    }

    std::vector<std::string> outboxMsgs = OfflineQueue::PopOutboxMessages();
    for (const auto& outboxMsg : outboxMsgs)
    {
      SetDialogMessage("Sending queued messages");

      Header header;
      header.SetData(outboxMsg);

      SmtpManager::Action smtpAction;
      smtpAction.m_CreatedMsg = outboxMsg;
      smtpAction.m_From = header.GetFrom();
      smtpAction.m_To = header.GetTo();
      smtpAction.m_Cc = header.GetCc();
      smtpAction.m_Bcc = header.GetBcc();
      smtpAction.m_IsSendCreatedMessage = true;
      smtpAction.m_FormatFlowed = (m_ComposeLineWrap == LineWrapFormatFlowed);

      m_SmtpManager->AsyncAction(smtpAction);
    }
  }
}

void Ui::StartComposeBackup()
{
  if (m_ComposeBackupInterval != 0)
  {
    m_ComposeBackupRunning = true;
    m_ComposeBackupThread = std::thread(&Ui::ComposeBackupProcess, this);
  }
}

void Ui::StopComposeBackup()
{
  if (m_ComposeBackupInterval != 0)
  {
    m_ComposeBackupRunning = false;
    {
      std::unique_lock<std::mutex> lock(m_ComposeBackupMutex);
      m_ComposeBackupCond.notify_one();
    }
    m_ComposeBackupThread.join();
  }
}

void Ui::ComposeBackupProcess()
{
  LOG_DEBUG("starting backup thread");
  while (m_ComposeBackupRunning)
  {
    bool timedOut = false;

    {
      std::unique_lock<std::mutex> lock(m_ComposeBackupMutex);
      if (m_ComposeBackupCond.wait_for(lock, std::chrono::seconds(m_ComposeBackupInterval)) ==
          std::cv_status::timeout)
      {
        timedOut = true;
      }
    }

    if (timedOut)
    {
      SmtpManager::Action smtpAction;
      smtpAction.m_IsCreateMessage = true;
      smtpAction.m_From = Util::ToString(GetComposeStr(HeaderFrom));
      smtpAction.m_To = Util::ToString(GetComposeStr(HeaderTo));
      smtpAction.m_Cc = Util::ToString(GetComposeStr(HeaderCc));
      smtpAction.m_Bcc = Util::ToString(GetComposeStr(HeaderBcc));
      smtpAction.m_Att = Util::ToString(GetComposeStr(HeaderAtt));
      smtpAction.m_Subject = Util::ToString(GetComposeStr(HeaderSub));
      smtpAction.m_Body = Util::ToString(GetComposeBodyForSend());
      smtpAction.m_HtmlBody = MakeHtmlPart(Util::ToString(m_ComposeMessageStr));
      smtpAction.m_RefMsgId = m_ComposeHeaderRef;

      SmtpManager::Result smtpResult = m_SmtpManager->SyncAction(smtpAction);
      if (smtpResult.m_SmtpStatus == SmtpStatusOk)
      {
        OfflineQueue::PushComposeMessage(smtpResult.m_Message);
        LOG_DEBUG("backup thread message saved");
      }
      else
      {
        LOG_WARNING("backup thread message creation failed");
      }
    }
  }

  OfflineQueue::PopComposeMessages();

  LOG_DEBUG("stopping backup thread");
}

std::map<std::string, uint32_t>& Ui::GetDisplayUids(const std::string& p_Folder)
{
  const SortFilter& sortFilter = m_SortFilter[p_Folder];
  std::map<std::string, uint32_t>& displayUids = m_DisplayUids[p_Folder][sortFilter];
  return displayUids;
}

std::set<uint32_t>& Ui::GetHeaderUids(const std::string& p_Folder)
{
  return m_HeaderUids[p_Folder];
}

std::string Ui::GetDisplayUidsKey(const std::string& p_Folder, uint32_t p_Uid, SortFilter p_SortFilter)
{
  std::map<uint32_t, Header>& headers = m_Headers[p_Folder];
  const std::map<uint32_t, uint32_t>& flags = m_Flags[p_Folder];

  std::string key;
  std::string priKey;
  std::map<uint32_t, Header>::iterator hit = headers.find(p_Uid);
  std::string dateUidKey =
    ((hit != headers.end()) ? hit->second.GetDateTime() : "") + " " + Util::ZeroPad(p_Uid, 7);
  std::map<uint32_t, uint32_t>::const_iterator fit;
  switch (p_SortFilter)
  {
    case SortDefault:
    case SortDateDesc:
      key = dateUidKey;
      break;

    case SortDateAsc:
      key = dateUidKey;
      Util::BitInvertString(key);
      break;

    case SortUnseenOnly:
      fit = flags.find(p_Uid);
      key = ((fit != flags.end()) && !Flag::GetSeen(fit->second)) ? dateUidKey : "";
      break;

    case SortAttchOnly:
      key = ((hit != headers.end()) && hit->second.GetHasAttachments()) ? dateUidKey : "";
      break;

    case SortCurrDateOnly:
      key = ((hit != headers.end()) && (hit->second.GetDate() == m_FilterCustomStr)) ? dateUidKey : "";
      break;

    case SortCurrNameOnly:
      if (hit != headers.end())
      {
        std::string name = (m_CurrentFolder != m_SentFolder) ? hit->second.GetShortFrom() : hit->second.GetShortTo();
        Util::NormalizeName(name);
        key = (name == m_FilterCustomStr) ? dateUidKey : "";
      }
      else
      {
        key = "";
      }
      break;

    case SortCurrSubjOnly:
      if (hit != headers.end())
      {
        std::string subj = hit->second.GetSubject();
        Util::NormalizeSubject(subj, true /*p_ToLower*/);
        key = (subj == m_FilterCustomStr) ? dateUidKey : "";
      }
      else
      {
        key = "";
      }
      break;

    case SortNameDesc:
      if (hit != headers.end())
      {
        priKey = (p_Folder != m_SentFolder) ? hit->second.GetShortFrom() : hit->second.GetShortTo();
      }
      else
      {
        priKey = "";
      }
      Util::NormalizeName(priKey);
      key = priKey + " " + dateUidKey;
      break;

    case SortNameAsc:
      if (hit != headers.end())
      {
        priKey = (p_Folder != m_SentFolder) ? hit->second.GetShortFrom() : hit->second.GetShortTo();
      }
      else
      {
        priKey = "";
      }
      Util::NormalizeName(priKey);
      key = priKey + " " + dateUidKey;
      Util::BitInvertString(key);
      break;

    case SortSubjDesc:
      priKey = ((hit != headers.end()) ? hit->second.GetSubject() : "");
      Util::NormalizeSubject(priKey, true /*p_ToLower*/);
      key = priKey + " " + dateUidKey;
      break;

    case SortSubjAsc:
      priKey = ((hit != headers.end()) ? hit->second.GetSubject() : "");
      Util::NormalizeSubject(priKey, true /*p_ToLower*/);
      key = priKey + " " + dateUidKey;
      Util::BitInvertString(key);
      break;

    case SortUnseenDesc:
      fit = flags.find(p_Uid);
      priKey = ((fit != flags.end()) && !Flag::GetSeen(fit->second)) ? "1" : "0";
      key = priKey + " " + dateUidKey;
      break;

    case SortUnseenAsc:
      fit = flags.find(p_Uid);
      priKey = ((fit != flags.end()) && !Flag::GetSeen(fit->second)) ? "1" : "0";
      key = priKey + " " + dateUidKey;
      Util::BitInvertString(key);
      break;

    case SortAttchDesc:
      priKey = ((hit != headers.end()) && hit->second.GetHasAttachments()) ? "1" : "0";
      key = priKey + " " + dateUidKey;
      break;

    case SortAttchAsc:
      priKey = ((hit != headers.end()) && hit->second.GetHasAttachments()) ? "1" : "0";
      key = priKey + " " + dateUidKey;
      Util::BitInvertString(key);
      break;

    default:
      LOG_WARNING("unhandled sortfilter %d", p_SortFilter);
      break;
  }

  return key;
}

// must be called with m_Mutex lock held
void Ui::UpdateDisplayUids(const std::string& p_Folder,
                           const std::set<uint32_t>& p_RemovedUids /*= std::set<uint32_t>()*/,
                           const std::set<uint32_t>& p_AddedUids /*= std::set<uint32_t>()*/,
                           bool p_FilterUpdated /*= false*/)
{
  std::set<uint32_t>& headerUids = m_HeaderUids[p_Folder];
  SortFilter& sortFilter = m_SortFilter[p_Folder];
  std::map<std::string, uint32_t>& displayUids = m_DisplayUids[p_Folder][sortFilter];
  uint64_t& displayUidsVersion = m_DisplayUidsVersion[p_Folder][sortFilter];
  uint64_t& headerUidsVersion = m_HeaderUidsVersion[p_Folder];
  (void)p_FilterUpdated; // @todo: remove unused argument

  if (displayUidsVersion != headerUidsVersion)
  {
    displayUids.clear();
    for (auto& uid : headerUids)
    {
      if (uid == 0) continue;

      std::string key = GetDisplayUidsKey(p_Folder, uid, sortFilter);
      if (key.empty()) continue;

      displayUids.insert(std::pair<std::string, uint32_t>(key, uid));
    }

    displayUidsVersion = headerUidsVersion;
  }

  if (!p_RemovedUids.empty())
  {
    headerUids = headerUids - p_RemovedUids;
    ++headerUidsVersion;

    for (auto& uid : p_RemovedUids)
    {
      if (uid == 0) continue;

      std::string key = GetDisplayUidsKey(p_Folder, uid, sortFilter);
      if (key.empty()) continue;

      auto displayUid = displayUids.find(key);
      if (displayUid != displayUids.end())
      {
        displayUids.erase(displayUid);
      }
    }

    displayUidsVersion = headerUidsVersion;
  }

  if (!p_AddedUids.empty())
  {
    headerUids = headerUids + p_AddedUids;
    ++headerUidsVersion;

    for (auto& uid : p_AddedUids)
    {
      if (uid == 0) continue;

      std::string key = GetDisplayUidsKey(p_Folder, uid, sortFilter);
      if (key.empty()) continue;

      displayUids.insert(std::pair<std::string, uint32_t>(key, uid));
    }

    displayUidsVersion = headerUidsVersion;
  }
}

void Ui::SortFilterPreUpdate()
{
  if (m_PersistSelectionOnSortFilterChange)
  {
    UpdateUidFromIndex(true /* p_UserTriggered */);
  }
}

void Ui::SortFilterUpdated(bool p_FilterUpdated)
{
  {
    std::lock_guard<std::mutex> lock(m_Mutex);
    UpdateDisplayUids(m_CurrentFolder, std::set<uint32_t>(), std::set<uint32_t>(), p_FilterUpdated);
  }

  if (m_PersistSelectionOnSortFilterChange)
  {
    UpdateIndexFromUid();
  }
  else
  {
    m_MessageListCurrentIndex[m_CurrentFolder] = 0;
    UpdateUidFromIndex(true /* p_UserTriggered */);
  }
}

void Ui::DisableSortFilter()
{
  SortFilter& sortFilter = m_SortFilter[m_CurrentFolder];
  bool wasFilterEnabled = (sortFilter == SortUnseenOnly) || (sortFilter == SortAttchOnly);
  SortFilterPreUpdate();
  sortFilter = SortDefault;
  SortFilterUpdated(wasFilterEnabled);
}

void Ui::ToggleFilter(SortFilter p_SortFilter)
{
  SortFilter& sortFilter = m_SortFilter[m_CurrentFolder];
  SortFilterPreUpdate();
  SortFilter newSortFilter = (sortFilter != p_SortFilter) ? p_SortFilter : SortDefault;

  if ((newSortFilter == SortCurrDateOnly) || (newSortFilter == SortCurrNameOnly) ||
      (newSortFilter == SortCurrSubjOnly))
  {
    const int uid = m_CurrentFolderUid.second;
    std::map<uint32_t, Header>& headers = m_Headers[m_CurrentFolder];
    std::map<uint32_t, Header>::iterator hit = headers.find(uid);
    if (hit == headers.end())
    {
      SetDialogMessage("No message selected to filter on");
      return;
    }

    // dont cache custom date, name or subject filters due to mem usage
    std::map<std::string, uint32_t>& displayUids = m_DisplayUids[m_CurrentFolder][newSortFilter];
    uint64_t& displayUidsVersion = m_DisplayUidsVersion[m_CurrentFolder][newSortFilter];
    displayUids.clear();
    displayUidsVersion = 0;

    switch (newSortFilter)
    {
      case SortCurrDateOnly:
        m_FilterCustomStr = hit->second.GetDate();
        break;

      case SortCurrNameOnly:
        m_FilterCustomStr = (m_CurrentFolder != m_SentFolder) ? hit->second.GetShortFrom() : hit->second.GetShortTo();
        Util::NormalizeName(m_FilterCustomStr);
        break;

      case SortCurrSubjOnly:
        m_FilterCustomStr = hit->second.GetSubject();
        Util::NormalizeSubject(m_FilterCustomStr, true /*p_ToLower*/);
        break;

      default:
        break;
    }
  }

  sortFilter = newSortFilter;
  SortFilterUpdated(true /* p_FilterUpdated */);
}

void Ui::ToggleSort(SortFilter p_SortFirst, SortFilter p_SortSecond)
{
  SortFilter& sortFilter = m_SortFilter[m_CurrentFolder];
  bool wasFilterEnabled = (sortFilter == SortUnseenOnly) || (sortFilter == SortAttchOnly);
  SortFilterPreUpdate();
  sortFilter = (sortFilter == p_SortSecond) ? SortDefault : ((sortFilter == p_SortFirst) ? p_SortSecond : p_SortFirst);
  SortFilterUpdated(wasFilterEnabled);
}

const std::vector<std::wstring>& Ui::GetCachedWordWrapLines(const std::string& p_Folder, uint32_t p_Uid)
{
  static std::string prevFolder;
  static uint32_t prevUid = 0;
  static bool prevPlaintext = false;
  static bool prevProcessFlowed = false;
  static int32_t prevMaxViewLineLength = 0;
  static std::vector<std::wstring> wlines;
  static size_t prevTextLen = 0;

  if (p_Folder.empty() && (p_Uid == 0)) return wlines;

  if ((prevFolder == p_Folder) && (prevUid == p_Uid) && (prevPlaintext == m_Plaintext) &&
      (prevProcessFlowed == m_CurrentMessageProcessFlowed) && (prevMaxViewLineLength == m_MaxViewLineLength) &&
      (prevTextLen == m_CurrentMessageViewText.size()))
  {
    return wlines;
  }

  prevFolder = p_Folder;
  prevUid = p_Uid;
  prevPlaintext = m_Plaintext;
  prevProcessFlowed = m_CurrentMessageProcessFlowed;
  prevMaxViewLineLength = m_MaxViewLineLength;
  prevTextLen = m_CurrentMessageViewText.size(); // cater for search results async header load

  const std::wstring wtext = Util::ToWString(m_CurrentMessageViewText);
  const bool outputFlowed = false; // only generate when sending after compose
  const bool quoteWrap = m_RewrapQuotedLines;
  const int expandTabSize = m_TabSize; // enabled
  wlines = Util::WordWrap(wtext, m_MaxViewLineLength, m_CurrentMessageProcessFlowed,
                          outputFlowed, quoteWrap, expandTabSize);
  wlines.push_back(L"");

  size_t wlinesSize = wlines.size();
  for (size_t i = 0; i < wlinesSize; ++i)
  {
    if (wlines[i].empty())
    {
      m_MessageViewHeaderLineCount = i;
      break;
    }
  }

  return wlines;
}

void Ui::ClearSelection()
{
  m_SelectedUids.clear();
  m_AllSelected = false;
}

void Ui::ToggleSelected()
{
  const std::string& folder = m_CurrentFolderUid.first;
  const int uid = m_CurrentFolderUid.second;

  std::set<uint32_t>& folderSelectedUids = m_SelectedUids[folder];
  auto it = folderSelectedUids.find(uid);
  if (it == folderSelectedUids.end())
  {
    folderSelectedUids.insert(uid);
    SetDialogMessage("Selected message");
  }
  else
  {
    folderSelectedUids.erase(uid);
    if (folderSelectedUids.empty())
    {
      m_SelectedUids.erase(folder);
    }

    SetDialogMessage("Unselected message");
  }
}

void Ui::ToggleSelectAll()
{
  m_SelectedUids.clear();
  if (m_AllSelected)
  {
    m_AllSelected = false;
    SetDialogMessage("Unselected all");
    return;
  }

  int selectCount = 0;
  if (m_MessageListSearch)
  {
    std::lock_guard<std::mutex> lock(m_SearchMutex);
    std::vector<Header>& headers = m_MessageListSearchResultHeaders;
    int idxMax = headers.size();
    for (int i = 0; i < idxMax; ++i)
    {
      const std::string& folder = m_MessageListSearchResultFolderUids.at(i).first;
      const int uid = m_MessageListSearchResultFolderUids.at(i).second;
      m_SelectedUids[folder].insert(uid);
      ++selectCount;
    }
  }
  else
  {
    std::set<uint32_t>& folderSelectedUids = m_SelectedUids[m_CurrentFolder];

    std::lock_guard<std::mutex> lock(m_Mutex);
    const std::map<std::string, uint32_t>& displayUids = GetDisplayUids(m_CurrentFolder);
    for (auto& displayUid : displayUids)
    {
      folderSelectedUids.insert(displayUid.second);
      ++selectCount;
    }
  }

  SetDialogMessage("Selected all " + std::to_string(selectCount) + " messages");
  m_AllSelected = true;
}

int Ui::GetSelectedCount()
{
  int selectCount = 0;
  for (auto& selectedFolder : m_SelectedUids)
  {
    selectCount += selectedFolder.second.size();
  }

  return selectCount;
}

std::string Ui::GetBodyText(Body& p_Body)
{
  if (!m_Plaintext)
  {
    if (p_Body.ParseHtmlIfNeeded())
    {
      const std::string& folder = m_CurrentFolderUid.first;
      const int uid = m_CurrentFolderUid.second;

      ImapManager::Action imapAction;
      imapAction.m_Folder = folder;
      imapAction.m_UpdateCache = true;
      imapAction.m_SetBodysCache[uid] = p_Body;
      m_ImapManager->AsyncAction(imapAction);
    }
  }
  
  std::string bodyText = m_Plaintext ? p_Body.GetTextPlain() : p_Body.GetTextHtml();
  
  // Use HTML parser for better rendering if available and not in plain text mode
  if (m_HtmlParser && !m_Plaintext && !p_Body.GetTextHtml().empty()) {
    auto formattedContent = m_HtmlParser->ParseHtmlToTerminal(p_Body.GetTextHtml());
    // Convert formatted content back to plain text for now
    // This could be enhanced to preserve formatting
    std::string formattedText;
    for (const auto& format : formattedContent) {
      formattedText += format.text;
    }
    if (!formattedText.empty()) {
      bodyText = formattedText;
    }
  }
  
  return bodyText;
}

void Ui::FilePickerOrStateFileList()
{
  std::string filePickerCmd = Util::GetFilePickerCmd();
  if (filePickerCmd.empty())
  {
    SetState(StateFileList);
  }
  else
  {
    endwin();

    std::string outPath = Util::GetTempFilename(".txt");
    std::string command = filePickerCmd + " > " + outPath;
    if (system(command.c_str()) == 0)
    {
      std::string filesStr = Util::ReadFile(outPath);
      if (!filesStr.empty())
      {
        std::vector<std::string> files = Util::Split(filesStr, '\n');
        for (const auto& file : files)
        {
          AddAttachmentPath(file);
        }
      }
    }
    else
    {
      LOG_WARNING("external command failed: %s", command.c_str());
    }

    Util::DeleteFile(outPath);

    refresh();
    wint_t key = 0;
    while (get_wch(&key) != ERR)
    {
      // Discard any remaining input
    }
  }
}

void Ui::AddAttachmentPath(const std::string& p_Path)
{
  if (p_Path.empty()) return;

  std::string filepaths;
  const std::string& oldFilepaths = Util::Trim(Util::ToString(m_ComposeHeaderStr[m_ComposeHeaderLine]));

  const std::string newPath = Util::EscapePath(p_Path);
  if (oldFilepaths.empty())
  {
    filepaths = newPath;
  }
  else if (oldFilepaths[oldFilepaths.size() - 1] != ',')
  {
    filepaths = ", " + newPath;
  }
  else
  {
    filepaths = " " + newPath;
  }

  m_ComposeHeaderStr[m_ComposeHeaderLine] = Util::ToWString(oldFilepaths + filepaths);
  m_ComposeHeaderPos = m_ComposeHeaderStr[m_ComposeHeaderLine].size();
}

void Ui::AddAddress(const std::string& p_Address)
{
  std::string addAddress;
  const std::string& oldAddress = Util::Trim(Util::ToString(m_ComposeHeaderStr[m_ComposeHeaderLine]));
  if (oldAddress.empty())
  {
    addAddress = p_Address;
  }
  else if (oldAddress[oldAddress.size() - 1] != ',')
  {
    addAddress = ", " + p_Address;
  }
  else
  {
    addAddress = " " + p_Address;
  }

  m_ComposeHeaderStr[m_ComposeHeaderLine] = Util::ToWString(oldAddress + addAddress);
  m_ComposeHeaderPos = m_ComposeHeaderStr[m_ComposeHeaderLine].size();
}

void Ui::SetAddress(const std::string& p_Address)
{
  m_ComposeHeaderStr[m_ComposeHeaderLine] = Util::ToWString(p_Address);
  m_ComposeHeaderPos = m_ComposeHeaderStr[m_ComposeHeaderLine].size();
}

std::string Ui::GetDefaultFrom()
{
  static const Contact defaultFrom(m_Address, m_Name);
  return defaultFrom.ToString();
}

std::wstring Ui::GetSignatureStr(bool p_NoPrefix /*= false*/)
{
  if (!m_Signature)
  {
    return std::wstring();
  }

  std::string signatureStr;
  static const std::string sigPriPath = Util::GetApplicationDir() + "signature.txt";
  static const std::string sigSecPath = std::string(getenv("HOME")) + "/.signature";
  if (Util::IsReadableFile(sigPriPath))
  {
    signatureStr = Util::ReadFile(sigPriPath);
  }
  else if (Util::IsReadableFile(sigSecPath))
  {
    signatureStr = Util::ReadFile(sigSecPath);
  }
  else
  {
    LOG_WARNING("signature file missing");
    return std::wstring();
  }

  if (signatureStr.empty())
  {
    LOG_WARNING("signature file is empty");
    return std::wstring();
  }

  if (!p_NoPrefix)
  {
    signatureStr = "\n\n" + signatureStr; // prefix with line breaks
  }

  std::wstring signatureWStr = Util::ToWString(signatureStr);
  Util::StripCR(signatureWStr);

  return signatureWStr;
}

bool Ui::HandleListKey(int p_Key, int& p_Index)
{
  if (p_Key == KEY_UP)
  {
    --p_Index;
  }
  else if (p_Key == KEY_DOWN)
  {
    ++p_Index;
  }
  else if (p_Key == m_KeyPrevPage)
  {
    p_Index = p_Index - m_MainWinHeight;
  }
  else if (p_Key == m_KeyNextPage)
  {
    p_Index = p_Index + m_MainWinHeight;
  }
  else if (p_Key == KEY_HOME)
  {
    p_Index = 0;
  }
  else if (p_Key == KEY_END)
  {
    p_Index = std::numeric_limits<int>::max() - 1;
  }
  else
  {
    return false;
  }

  return true;
}

bool Ui::HandleLineKey(int p_Key, std::wstring& p_Str, int& p_Pos)
{
  if (p_Key == KEY_LEFT)
  {
    p_Pos = Util::Bound(0, p_Pos - 1, (int)p_Str.size());
  }
  else if (p_Key == KEY_RIGHT)
  {
    p_Pos = Util::Bound(0, p_Pos + 1, (int)p_Str.size());
  }
  else if ((p_Key == KEY_BACKSPACE) || (p_Key == KEY_DELETE))
  {
    if (p_Pos > 0)
    {
      p_Str.erase(--p_Pos, 1);
    }
  }
  else if ((p_Key == KEY_DC) || (p_Key == m_KeyDeleteCharAfterCursor))
  {
    if (p_Pos < (int)p_Str.size())
    {
      p_Str.erase(p_Pos, 1);
    }
  }
  else if (p_Key == m_KeyDeleteLineAfterCursor)
  {
    Util::DeleteToNextMatch(p_Str, p_Pos, 0, L"\n");
  }
  else if (p_Key == m_KeyDeleteLineBeforeCursor)
  {
    Util::DeleteToPrevMatch(p_Str, p_Pos, -1, L"\n");
  }
  else if (p_Key == m_KeyBeginLine)
  {
    Util::JumpToPrevMatch(p_Str, p_Pos, -1, L"\n");
  }
  else if (p_Key == m_KeyEndLine)
  {
    Util::JumpToNextMatch(p_Str, p_Pos, 0, L"\n");
  }
  else if (p_Key == m_KeyBackwardWord)
  {
    Util::JumpToPrevMatch(p_Str, p_Pos, -2, L" \n");
  }
  else if (p_Key == m_KeyForwardWord)
  {
    Util::JumpToNextMatch(p_Str, p_Pos, 1, L" \n");
  }
  else if (p_Key == m_KeyBackwardKillWord)
  {
    Util::DeleteToPrevMatch(p_Str, p_Pos, -1, L" \n");
  }
  else if (p_Key == m_KeyKillWord)
  {
    Util::DeleteToNextMatch(p_Str, p_Pos, 0, L" \n");
  }
  else
  {
    return false;
  }

  return true;
}

bool Ui::HandleTextKey(int p_Key, std::wstring& p_Str, int& p_Pos)
{
  if (IsValidTextKey(p_Key))
  {
    p_Str.insert(p_Pos++, 1, p_Key);
  }
  else
  {
    return false;
  }

  return true;
}

bool Ui::HandleDocKey(int p_Key, std::wstring& p_Str, int& p_Pos)
{
  if (p_Key == KEY_HOME)
  {
    p_Pos = 0;
  }
  else if (p_Key == KEY_END)
  {
    p_Pos = p_Str.size();
  }
  else
  {
    return false;
  }

  return true;
}

bool Ui::HandleComposeKey(int p_Key)
{
  if (p_Key == m_KeyCancel)
  {
    if (m_CancelWithoutConfirm || Ui::PromptYesNo("Cancel message (y/n)?"))
    {
      Util::RmDir(Util::GetPreviewTempDir());
      UpdateUidFromIndex(true /* p_UserTriggered */);
      SetLastStateOrMessageList();
      Util::RmDir(m_ComposeTempDirectory);
      StopComposeBackup();
    }
  }
  else if (p_Key == m_KeySend)
  {
    if (ComposedMessageIsValid(true /* p_ForSend */) &&
        (m_SendWithoutConfirm || Ui::PromptYesNo("Send message (y/n)?")))
    {
      Util::RmDir(Util::GetPreviewTempDir());
      SendComposedMessage();
      UpdateUidFromIndex(true /* p_UserTriggered */);
      if (m_ComposeDraftUid != 0)
      {
        SetState(StateViewMessageList);
      }
      else
      {
        SetLastStateOrMessageList();
      }

      StopComposeBackup();
    }
  }
  else if (p_Key == m_KeyPostpone)
  {
    if (ComposedMessageIsValid(false /* p_ForSend */) &&
        (m_PostponeWithoutConfirm || Ui::PromptYesNo("Postpone message (y/n)?")))
    {
      Util::RmDir(Util::GetPreviewTempDir());
      UploadDraftMessage();
      UpdateUidFromIndex(true /* p_UserTriggered */);
      if (m_ComposeDraftUid != 0)
      {
        SetState(StateViewMessageList);
      }
      else
      {
        SetLastStateOrMessageList();
      }

      StopComposeBackup();
    }
  }
  else if (p_Key == m_KeyExtEditor)
  {
    const std::string editorCmd = Util::GetEditorCmd();
    ExtEditor(editorCmd, m_ComposeMessageStr, m_ComposeMessagePos);
  }
  else if (p_Key == m_KeyRichHeader)
  {
    std::wstring from = GetComposeStr(HeaderFrom);
    std::wstring to = GetComposeStr(HeaderTo);
    std::wstring cc = GetComposeStr(HeaderCc);
    std::wstring bcc = GetComposeStr(HeaderBcc);
    std::wstring att = GetComposeStr(HeaderAtt);
    std::wstring sub = GetComposeStr(HeaderSub);

    m_ShowRichHeader = !m_ShowRichHeader;

    SetComposeStr(HeaderAll, L"");
    SetComposeStr(HeaderFrom, Util::ToWString(GetDefaultFrom()));
    SetComposeStr(HeaderTo, to);
    SetComposeStr(HeaderCc, cc);
    SetComposeStr(HeaderBcc, bcc);
    SetComposeStr(HeaderAtt, att);
    SetComposeStr(HeaderSub, sub);
  }
  else if (p_Key == m_KeyExtHtmlPreview)
  {
    if (m_CurrentMarkdownHtmlCompose)
    {
      std::string tempFilePath = Util::GetPreviewTempDir() + "msg.html";
      std::string htmlStr = MakeHtmlPart(Util::ToString(m_ComposeMessageStr));
      Util::WriteFile(tempFilePath, htmlStr);
      ExtHtmlPreview(tempFilePath);
    }
    else
    {
      SetDialogMessage("Markdown compose is not enabled");
    }
  }
  else if (p_Key == m_KeyToggleMarkdownCompose)
  {
    m_CurrentMarkdownHtmlCompose = !m_CurrentMarkdownHtmlCompose;
  }
  else if (p_Key == m_KeySpell)
  {
    const std::string spellCmd = Util::GetSpellCmd();
    if (!spellCmd.empty())
    {
      size_t quoteStartPos =
        !m_ComposeQuotedStart.empty() ? m_ComposeMessageStr.find(Util::ToWString(m_ComposeQuotedStart))
                                      : std::string::npos;
      if (quoteStartPos == std::string::npos)
      {
        ExtEditor(spellCmd, m_ComposeMessageStr, m_ComposeMessagePos);
      }
      else
      {
        std::wstring messageComposed = m_ComposeMessageStr.substr(0, quoteStartPos);
        std::wstring messageQuoted = m_ComposeMessageStr.substr(quoteStartPos);
        ExtEditor(spellCmd, messageComposed, m_ComposeMessagePos);
        m_ComposeMessageStr = messageComposed + messageQuoted;
      }
    }
    else
    {
      SetDialogMessage("Spell command not found", true);
    }
  }
  else
  {
    return false;
  }

  return true;
}

void Ui::OnWakeUp()
{
  LOG_DEBUG_FUNC(STR());

  // Dummy request to trigger exit of idle
  ImapManager::Request request;
  LOG_DEBUG("async req none");
  m_ImapManager->AsyncRequest(request);
}

void Ui::AutoMoveSelectFolder()
{
  LOG_DEBUG_FUNC(STR());

  std::string subject;
  std::string sender;
  std::string folder;
  std::string selfSender = m_Name;
  std::string foundFolder;
  static const int maxSearchCount = 10;
  static const int minLengthPrefix = 8;

  {
    std::lock_guard<std::mutex> lock(m_Mutex);
    uint32_t uid = 0;
    auto selectedUidsIt = m_SelectedUids.find(m_CurrentFolder);
    if ((selectedUidsIt != m_SelectedUids.end()) && (!selectedUidsIt->second.empty()))
    {
      folder = m_CurrentFolder;
      uid = *(selectedUidsIt->second.begin());
    }
    else
    {
      folder = m_CurrentFolderUid.first;
      uid = m_CurrentFolderUid.second;
    }

    std::map<uint32_t, Header>& headers = m_Headers[folder];
    std::map<uint32_t, Header>::iterator hit = headers.find(uid);
    if (hit != headers.end())
    {
      subject = Util::Trim(hit->second.GetSubject());
      sender = Util::Trim(((folder != m_SentFolder) ? hit->second.GetShortFrom() : hit->second.GetShortTo()));
    }
  }

  Util::NormalizeSubject(subject, true /*p_ToLower*/);
  Util::NormalizeName(sender);
  Util::NormalizeName(selfSender);
  const std::string subjectPrefix = subject.substr(0, subject.find(" ", minLengthPrefix));

  static const std::string s_QueryCommonBase = [&]()
  {
    std::string queryCommonBase;
    if (!m_SentFolder.empty())
    {
      queryCommonBase += " AND NOT folder:\"" + m_SentFolder + "\"";
    }

    if (!m_TrashFolder.empty())
    {
      queryCommonBase += " AND NOT folder:\"" + m_TrashFolder + "\"";
    }

    return queryCommonBase;
  }();

  const std::string queryCommon = s_QueryCommonBase + (!folder.empty() ? " AND NOT folder:\"" + folder + "\"" : "");

  std::vector<std::string> queryStrs;
  if (!subject.empty())
  {
    // full subject
    queryStrs.push_back("subject:\"" + subject + "\"" + queryCommon);
  }

  if (!subjectPrefix.empty() && !sender.empty())
  {
    // subject prefix and sender name
    queryStrs.push_back("subject:\"" + subjectPrefix + "*\" AND from:\"" + sender + "\"" + queryCommon);
  }

  if (!subjectPrefix.empty())
  {
    // subject prefix
    queryStrs.push_back("subject:\"" + subjectPrefix + "*\"" + queryCommon);
  }

  if (!sender.empty())
  {
    // sender name
    queryStrs.push_back("from:\"" + sender + "\"" + queryCommon);
  }

  if (!queryStrs.empty())
  {
    for (const auto& queryStr : queryStrs)
    {
      ImapManager::SearchQuery searchQuery(queryStr, 0, maxSearchCount);
      ImapManager::SearchResult searchResult;
      m_ImapManager->SyncSearch(searchQuery, searchResult);
      if (searchResult.m_FolderUids.empty())
      {
        LOG_DEBUG("no matches for query %s", queryStr.c_str());
      }
      else
      {
        foundFolder = searchResult.m_FolderUids.begin()->first;
        LOG_DEBUG("found %s for query %s", foundFolder.c_str(), queryStr.c_str());
        break;
      }
    }
  }
  else
  {
    LOG_DEBUG("skip search subject \"%s\" sender \"%s\"", subject.c_str(), sender.c_str());
  }

  m_FolderListFilterPos = 0;
  m_FolderListFilterStr.clear();
  m_FolderListCurrentFolder = !foundFolder.empty() ? foundFolder : folder;
  m_FolderListCurrentIndex = INT_MAX;
}

// Enhanced UI helper functions
std::vector<MessageDisplayInfo> Ui::ConvertToMessageDisplayInfo(const std::string& folder)
{
  std::vector<MessageDisplayInfo> messages;
  
  std::lock_guard<std::mutex> lock(m_Mutex);
  const std::map<std::string, uint32_t>& displayUids = GetDisplayUids(folder);
  const std::map<uint32_t, Header>& headers = m_Headers[folder];
  const std::map<uint32_t, uint32_t>& flags = m_Flags[folder];
  const std::string& currentDate = Header::GetCurrentDate();
  
  for (const auto& item : displayUids) {
    uint32_t uid = item.second;
    MessageDisplayInfo msgInfo;
    
    auto headerIt = headers.find(uid);
    if (headerIt != headers.end()) {
      const Header& header = headerIt->second;
      msgInfo.subject = header.GetSubject();
      msgInfo.sender = (folder == m_SentFolder) ? header.GetShortTo() : header.GetShortFrom();
      msgInfo.date = header.GetDateOrTime(currentDate);
      msgInfo.hasAttachments = header.GetHasAttachments();
      
      // Get a preview of the message
      const std::map<uint32_t, Body>& bodys = m_Bodys[folder];
      auto bodyIt = bodys.find(uid);
      if (bodyIt != bodys.end()) {
        const Body& body = bodyIt->second;
        std::string bodyText = m_Plaintext ? body.GetTextPlain() : body.GetTextHtml();
        // Extract first 100 characters as preview
        msgInfo.preview = bodyText.substr(0, std::min(size_t(100), bodyText.length()));
        // Remove newlines and extra whitespace
        std::replace(msgInfo.preview.begin(), msgInfo.preview.end(), '\n', ' ');
        std::replace(msgInfo.preview.begin(), msgInfo.preview.end(), '\r', ' ');
      }
    }
    
    auto flagIt = flags.find(uid);
    if (flagIt != flags.end()) {
      msgInfo.isUnread = !Flag::GetSeen(flagIt->second);
    } else {
      msgInfo.isUnread = true; // Default to unread if no flag info
    }
    
    msgInfo.folder = folder;
    messages.push_back(msgInfo);
  }
  
  return messages;
}

// Beautiful UI Enhancement Methods
void Ui::InitBeautifulColors()
{
  if (!m_ColorsEnabled) return;
  
  // Initialize beautiful color pairs with new enum names
  init_pair(COLOR_BEAUTIFUL_HEADER, COLOR_BLACK, COLOR_WHITE);  // High contrast: black text on white background
  init_pair(COLOR_ACTIVE_ITEM, COLOR_BLACK, COLOR_WHITE);       // High contrast: black text on white background
  init_pair(COLOR_SELECTED_ITEM, COLOR_WHITE, COLOR_MAGENTA);   // High contrast: white text on magenta background
  init_pair(COLOR_UNREAD_ITEM, COLOR_YELLOW, COLOR_BLACK);
  init_pair(COLOR_NORMAL_ITEM, COLOR_WHITE, COLOR_BLACK);
  init_pair(COLOR_FOLDER_ITEM, COLOR_BLUE, COLOR_BLACK);
  init_pair(COLOR_DATE_TIME, COLOR_GREEN, COLOR_BLACK);
  init_pair(COLOR_SENDER_NAME, COLOR_WHITE, COLOR_BLACK);
  init_pair(COLOR_SUBJECT_TEXT, COLOR_YELLOW, COLOR_BLACK);
  init_pair(COLOR_ACTIVE_SUBJECT, COLOR_BLACK, COLOR_WHITE);    // High contrast: black text on white background
  init_pair(COLOR_UNREAD_SUBJECT, COLOR_YELLOW, COLOR_BLACK);
  init_pair(COLOR_HEADER_NAME, COLOR_CYAN, COLOR_BLACK);
  init_pair(COLOR_HEADER_VALUE, COLOR_WHITE, COLOR_BLACK);
  init_pair(COLOR_QUOTED_TEXT, COLOR_BLUE, COLOR_BLACK);
  init_pair(COLOR_URL_LINK, COLOR_CYAN, COLOR_BLACK);
  init_pair(COLOR_MESSAGE_TEXT, COLOR_WHITE, COLOR_BLACK);
  init_pair(COLOR_SEARCH_MATCH, COLOR_BLACK, COLOR_YELLOW);
  init_pair(COLOR_ATTACHMENT_INFO, COLOR_MAGENTA, COLOR_BLACK);
  init_pair(COLOR_MESSAGE_BACKGROUND, COLOR_WHITE, COLOR_BLACK);
  
  // Store color pairs in array
  m_BeautifulColors[COLOR_BEAUTIFUL_HEADER] = COLOR_PAIR(COLOR_BEAUTIFUL_HEADER);
  m_BeautifulColors[COLOR_ACTIVE_ITEM] = COLOR_PAIR(COLOR_ACTIVE_ITEM);
  m_BeautifulColors[COLOR_SELECTED_ITEM] = COLOR_PAIR(COLOR_SELECTED_ITEM);
  m_BeautifulColors[COLOR_UNREAD_ITEM] = COLOR_PAIR(COLOR_UNREAD_ITEM);
  m_BeautifulColors[COLOR_NORMAL_ITEM] = COLOR_PAIR(COLOR_NORMAL_ITEM);
  m_BeautifulColors[COLOR_FOLDER_ITEM] = COLOR_PAIR(COLOR_FOLDER_ITEM);
  m_BeautifulColors[COLOR_DATE_TIME] = COLOR_PAIR(COLOR_DATE_TIME);
  m_BeautifulColors[COLOR_SENDER_NAME] = COLOR_PAIR(COLOR_SENDER_NAME);
  m_BeautifulColors[COLOR_SUBJECT_TEXT] = COLOR_PAIR(COLOR_SUBJECT_TEXT);
  m_BeautifulColors[COLOR_ACTIVE_SUBJECT] = COLOR_PAIR(COLOR_ACTIVE_SUBJECT);
  m_BeautifulColors[COLOR_UNREAD_SUBJECT] = COLOR_PAIR(COLOR_UNREAD_SUBJECT);
  m_BeautifulColors[COLOR_HEADER_NAME] = COLOR_PAIR(COLOR_HEADER_NAME);
  m_BeautifulColors[COLOR_HEADER_VALUE] = COLOR_PAIR(COLOR_HEADER_VALUE);
  m_BeautifulColors[COLOR_QUOTED_TEXT] = COLOR_PAIR(COLOR_QUOTED_TEXT);
  m_BeautifulColors[COLOR_URL_LINK] = COLOR_PAIR(COLOR_URL_LINK);
  m_BeautifulColors[COLOR_MESSAGE_TEXT] = COLOR_PAIR(COLOR_MESSAGE_TEXT);
  m_BeautifulColors[COLOR_SEARCH_MATCH] = COLOR_PAIR(COLOR_SEARCH_MATCH);
  m_BeautifulColors[COLOR_ATTACHMENT_INFO] = COLOR_PAIR(COLOR_ATTACHMENT_INFO);
  m_BeautifulColors[COLOR_MESSAGE_BACKGROUND] = COLOR_PAIR(COLOR_MESSAGE_BACKGROUND);
}

std::string Ui::GetUnicodeSymbol(UISymbols symbol)
{
  switch (symbol) {
    case SYMBOL_UNREAD: return "●";
    case SYMBOL_READ: return "○";
    case SYMBOL_SELECTED: return "✓";
    case SYMBOL_ATTACHMENT: return "📎";
    case SYMBOL_IMPORTANT: return "⭐";
    case SYMBOL_FOLDER: return "📁";
    case SYMBOL_INBOX: return "📥";
    case SYMBOL_SENT: return "📤";
    case SYMBOL_DRAFTS: return "📝";
    case SYMBOL_DRAFT: return "📝";
    case SYMBOL_TRASH: return "🗑";
    case SYMBOL_SPAM: return "⚠";
    case SYMBOL_ARCHIVE: return "📦";
    default: return " ";
  }
}

void Ui::ApplyBeautifulColors(WINDOW* window, BeautifulColors colorType)
{
  if (!m_ColorsEnabled) return;
  wattron(window, COLOR_PAIR(colorType));
}

void Ui::DrawBeautifulBorder(WINDOW* window, const std::string& title, bool isActive)
{
  if (!window) return;
  
  int height, width;
  getmaxyx(window, height, width);
  (void)height; // Suppress unused variable warning
  
  // Use beautiful colors for active/inactive borders
  if (isActive) {
    ApplyBeautifulColors(window, COLOR_BEAUTIFUL_HEADER);
  }
  
  // Draw a beautiful border with Unicode characters
  box(window, 0, 0);
  
  if (!title.empty()) {
    std::string decoratedTitle = "═══ " + title + " ═══";
    int titleStart = (width - decoratedTitle.length()) / 2;
    if (titleStart > 0 && titleStart + decoratedTitle.length() < static_cast<size_t>(width)) {
      mvwaddstr(window, 0, titleStart, decoratedTitle.c_str());
    }
  }
  
  if (isActive && m_ColorsEnabled) {
    wattroff(window, COLOR_PAIR(COLOR_BEAUTIFUL_HEADER));
  }
}

void Ui::DrawBeautifulTopBar()
{
  if (!m_TopWin) return;
  
  werase(m_TopWin);
  ApplyBeautifulColors(m_TopWin, COLOR_BEAUTIFUL_HEADER);
  
  // Create a beautiful title with icons
  std::string title = "Falaclient";
  int titleX = (m_ScreenWidth - title.length()) / 2;
  if (titleX > 0) {
    mvwaddstr(m_TopWin, 0, titleX, title.c_str());
  }
  
  // Add status information
  std::string statusInfo = GetStatusStr();
  if (!statusInfo.empty()) {
    int statusX = m_ScreenWidth - statusInfo.length() - 2;
    if (statusX > 0) {
      mvwaddstr(m_TopWin, 0, statusX, statusInfo.c_str());
    }
  }
  
  if (m_ColorsEnabled) {
    wattroff(m_TopWin, COLOR_PAIR(COLOR_BEAUTIFUL_HEADER));
  }
  
  wrefresh(m_TopWin);
}

void Ui::DrawBeautifulStatusLine(const std::string& status, const std::string& type)
{
  if (!m_DialogWin) return;
  
  // Store the beautiful status line information
  m_BeautifulStatusMessage = status;
  m_BeautifulStatusTime = std::chrono::system_clock::now();
  
  werase(m_DialogWin);
  
  // Use the same color scheme as the top bar for consistency
  BeautifulColors colorType = COLOR_BEAUTIFUL_HEADER;
  std::string icon = "ℹ ";
  
  if (type == "error") {
    colorType = COLOR_SEARCH_MATCH; // Use search match color for errors
    icon = "✗ ";
  } else if (type == "warning") {
    colorType = COLOR_ATTACHMENT_INFO; // Use attachment color for warnings
    icon = "⚠ ";
  } else if (type == "success") {
    colorType = COLOR_DATE_TIME; // Use date color for success
    icon = "✓ ";
  }
  
  // Use same styling as top bar - apply beautiful colors but without bold
  if (m_ColorsEnabled) {
    ApplyBeautifulColors(m_DialogWin, colorType);
  } else {
    // For non-color terminals, use the same attributes as top bar
    wattron(m_DialogWin, m_AttrsTopBar);
  }
  
  std::string fullMessage = icon + status;
  mvwaddstr(m_DialogWin, 0, 1, fullMessage.c_str());
  
  if (m_ColorsEnabled) {
    wattroff(m_DialogWin, COLOR_PAIR(colorType));
  } else {
    wattroff(m_DialogWin, m_AttrsTopBar);
  }
  
  wrefresh(m_DialogWin);
}

void Ui::DrawBeautifulProgressBar(int percentage, const std::string& operation)
{
  if (!m_DialogWin) return;
  
  int width = m_ScreenWidth - 4;
  int filled = (width * percentage) / 100;
  
  werase(m_DialogWin);
  ApplyBeautifulColors(m_DialogWin, COLOR_BEAUTIFUL_HEADER);
  
  // Add bold attribute for high contrast
  if (m_ColorsEnabled) {
    wattron(m_DialogWin, A_BOLD);
  }
  
  // Draw progress bar with Unicode blocks
  std::string progressBar = "[";
  for (int i = 0; i < width; ++i) {
    if (i < filled) {
      progressBar += "█";
    } else {
      progressBar += "░";
    }
  }
  progressBar += "] " + std::to_string(percentage) + "%";
  
  if (!operation.empty()) {
    progressBar += " " + operation;
  }
  
  mvwaddstr(m_DialogWin, 0, 1, progressBar.c_str());
  
  if (m_ColorsEnabled) {
    wattroff(m_DialogWin, A_BOLD);
    wattroff(m_DialogWin, COLOR_PAIR(COLOR_BEAUTIFUL_HEADER));
  }
  
  wrefresh(m_DialogWin);
}

std::string Ui::FormatBeautifulSize(size_t bytes)
{
  const char* suffixes[] = {"B", "KB", "MB", "GB", "TB"};
  double size = static_cast<double>(bytes);
  int i = 0;
  
  while (size >= 1024 && i < 4) {
    size /= 1024;
    i++;
  }
  
  std::ostringstream ss;
  if (i == 0) {
    ss << static_cast<int>(size) << " " << suffixes[i];
  } else {
    ss << std::fixed << std::setprecision(1) << size << " " << suffixes[i];
  }
  return ss.str();
}

std::string Ui::FormatBeautifulDate(const std::string& timestamp)
{
  if (timestamp.length() >= 16) {
    // Format: "MM/DD HH:MM" for beautiful display
    return timestamp.substr(5, 5) + " " + timestamp.substr(11, 5);
  } else if (timestamp.length() >= 10) {
    return timestamp.substr(5, 5); // Just MM/DD
  }
  return timestamp;
}

// Enhanced UI utility methods
std::string Ui::TruncateWithEllipsis(const std::string& text, int maxLength)
{
  if (static_cast<int>(text.length()) <= maxLength) {
    return text;
  }
  return text.substr(0, maxLength - 3) + "...";
}

std::string Ui::FormatTimestamp(const std::string& timestamp)
{
  // Simple timestamp formatting - can be enhanced
  if (timestamp.length() >= 10) {
    return timestamp.substr(5, 5); // MM-DD format
  }
  return timestamp;
}

std::string Ui::WrapTextToWidth(const std::string& text, int width)
{
  if (width <= 0) return text;
  
  std::string result;
  std::string currentLine;
  std::istringstream words(text);
  std::string word;
  
  while (words >> word) {
    if (currentLine.empty()) {
      currentLine = word;
    } else if (static_cast<int>(currentLine.length() + word.length() + 1) <= width) {
      currentLine += " " + word;
    } else {
      result += currentLine + "\n";
      currentLine = word;
    }
  }
  
  if (!currentLine.empty()) {
    result += currentLine;
  }
  
  return result;
}

std::string Ui::FormatFileSize(size_t bytes)
{
  const char* suffixes[] = {"B", "KB", "MB", "GB"};
  double size = static_cast<double>(bytes);
  int i = 0;
  
  while (size >= 1024 && i < 3) {
    size /= 1024;
    i++;
  }
  
  std::ostringstream ss;
  ss << std::fixed << std::setprecision(1) << size << " " << suffixes[i];
  return ss.str();
}
