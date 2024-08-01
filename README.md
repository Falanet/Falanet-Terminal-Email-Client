falanet
=====

| **Linux** | **Mac** |
|-----------|---------|

falanet is a terminal-based email client for Linux and macOS with a user interface
similar to alpine / pine.

Features
--------
- Support for IMAP and SMTP protocols
- Local cache using sqlite (optionally AES256-encrypted)
- Multi-threaded (email fetch and send done in background)
- Address book auto-generated based on email messages
- Viewing HTML emails (converted to text in terminal, or in external browser)
- Opening/viewing attachments in external program
- Simple setup wizard for Gmail, iCloud and Outlook/Hotmail
- UI similar to Alpine / Pine
- Compose message using external editor ($EDITOR)
- View message using external viewer ($PAGER)
- Saving and continuing draft messages
- Compose HTML emails using Markdown (see `markdown_html_compose` option)
- Email search
- Compose emails while offline
- Color customization
- Signature

Not Supported / Out of Scope
----------------------------
- Local mailbox downloaded by third-party application (OfflineIMAP, fdm, etc)
- Multiple email accounts in a single session
- Special handling for Gmail labels
- Threaded view


Usage
=====

Usage:

    falanet [OPTION]

Command-line Options:

    -d, --confdir <DIR>
        use a different directory than ~/.config/falanet

    -e, --verbose
        enable verbose logging

    -ee, --extra-verbose
        enable extra verbose logging

    -h, --help
        display this help and exit

    -k, --keydump
        key code dump mode

    -o, --offline
        run in offline mode

    -p, --pass
        change password

    -v, --version
        output version information and exit

    -x, --export <DIR>
        export cache to specified dir in Maildir format

Configuration files:

    ~/.config/falanet/auth.conf
        configures custom oauth2 client id and secret

    ~/.config/falanet/main.conf
        configures mail account and general settings

    ~/.config/falanet/ui.conf
        customizes UI settings

Examples:

    falanet -s gmail
        setup falanet for a gmail account


Supported Platforms
===================

falanet is developed and tested on Linux and macOS. Current version has been
tested on:

- macOS Sonoma 14.0
- Ubuntu 22.04 LTS

Build from Source
=================
**Get Source**

    git clone https://github.com/Falanet/Falanet-Terminal-Email-Client falanet && cd falanet

Using make.sh script
--------------------
If using macOS, Alpine, Arch, Fedora, Gentoo, Raspbian or Ubuntu, one can use
the `make.sh` script provided.

**Dependencies**

    ./make.sh deps

**Build / Install**

    ./make.sh build && ./make.sh install

Manually
--------
**Dependencies**

macOS

    brew install openssl ncurses xapian sqlite libmagic ossp-uuid

Arch

    sudo pacman -Sy cmake make openssl ncurses xapian-core sqlite cyrus-sasl curl expat zlib file pandoc

Debian-based (Ubuntu, Raspbian, etc)

    sudo apt install git cmake build-essential libssl-dev libreadline-dev libncurses5-dev libxapian-dev libsqlite3-dev libsasl2-dev libsasl2-modules libcurl4-openssl-dev libexpat-dev zlib1g-dev libmagic-dev uuid-dev

Fedora

    sudo yum -y install cmake openssl-devel ncurses-devel xapian-core-devel sqlite-devel cyrus-sasl-devel cyrus-sasl-plain libcurl-devel expat-devel zlib-devel file-devel libuuid-devel clang pandoc

Gentoo

    sudo emerge -n dev-util/cmake dev-libs/openssl sys-libs/ncurses dev-libs/xapian dev-db/sqlite dev-libs/cyrus-sasl net-misc/curl dev-libs/expat sys-libs/zlib sys-apps/file

**Build**

    mkdir -p build && cd build && cmake .. && make -s

**Install**

    sudo make install

Getting Started
===============

Falanet Email Provider
---------------------
Run falanet once in order for it to automatically generate the default config
file:

    $ falanet

Then open the config file `~/.config/falanet/main.conf` in your favourite text
editor and fill out the required fields:

    address=example@falanet.org
    drafts=Drafts
    imap_host=mail.falanet.org
    imap_port=993
    inbox=Inbox
    name=Firstname Lastname
    sent=Sent
    smtp_host=mail.falanet.org
    smtp_port=587
    trash=Trash
    user=example@falanet.org

Email Viewer
============

The email navigator / viewer supports the following commands:

    <              select folder
    >              view message / attachments
    p              previous message
    n              next message
    r              reply all
    R              reply to sender
    f              forward
    F              forward as attachment
    d              delete
    c              compose
    C              compose copy of message
    l              refresh current folder
    m              move with auto-selection of folder
    M              move without auto-selection of folder
    t              toggle unread
    v              view html part in external viewer
    x              export
    w              view message in external viewer
    i              import
    a              select all
    /              search
    <space>        select
    s              start full sync
    =              search messages with same subject
    -              search messages with same sender
    j              jump to message in search results

    !              sort by unread flag
    @              sort by attachment flag
    #              sort by date
    $              sort by sender name
    %              sort by subject

    1              filter by current message unread flag
    2              filter by current message attachment flag
    3              filter by current message date
    4              filter by current message sender name
    5              filter by current message subject

    `              filter reset


Compose Editor
==============

The built-in email compose editor in falanet supports the following:

    Alt-Backspace  delete previous word
    Alt-Delete     delete next word
    Alt-Left       move the cursor backward one word
    Alt-Right      move the cursor forward one word
    Arrow keys     move the cursor
    Backspace      backspace
    Ctrl-C         cancel message
    Ctrl-E         edit message in external editor
    Ctrl-K         delete current line
    Ctrl-N         toggle markdown editing
    Ctrl-O         postpone message
    Ctrl-R         toggle rich headers (bcc)
    Ctrl-T         to select, from address book / from file dialog
    Ctrl-V         preview html part (using markdown to html conversion)
    Ctrl-X         send message
    Delete         delete
    Enter          new line
    Page Up/Down   move the cursor page up / down

The email headers `To`, `Cc` and `Attchmnt` support comma-separated values, ex:

    To      : Alice <alice@example.com>, Bob <bob@example.com>
    Cc      : Chuck <chuck@example.com>, Dave <dave@example.com>
    Attchmnt: localpath.txt, /tmp/absolutepath.txt
    Subject : Hello world

Attachment paths may be local (just filename) or absolute (full path).


Email Search
============

Press `/` in the message list view to search the local cache for an email. The
local cache can be fully syncronized with server by pressing `s`. The search
engine supports queries with `"quoted strings"`, `+musthave`, `-mustnothave`,
`partialstring*`, `AND`, `OR`, `XOR` and `NOT`.

Search terms may be prefixed by `body:`, `subject:`, `from:`, `to:` or
`folder:` to search only specified fields. By default search query terms are
combined with `AND` unless specified. Results are sorted by email timestamp.

Press `<` or `Left` to exit search results and go back to current folder
message list.

Security
========

falanet caches data locally to improve performance. Cached data can be encrypted
by setting by setting `cache_encrypt=1` in main.conf. Message databases are
then encrypted using OpenSSL AES256-CBC with a key derived from a random salt
and the email account password. Folder names are hashed using SHA256 (thus not
encrypted).

Storing the account password (`save_pass=1` in main.conf) is *not* secure.
While falanet encrypts the password, the key is trivial to determine from
the source code. Only store the password if measurements are taken to ensure
`~/.config/falanet/secret.conf` cannot by accessed by a third-party.


Configuration
=============

Aside from `main.conf` covered above, the following files can be used to
configure falanet.

~/.config/falanet/ui.conf
-----------------------
This configuration file controls the UI aspects of falanet. Default configuration
file (platform-dependent defaults are left empty below):

    attachment_indicator=📎
    bottom_reply=0
    cancel_without_confirm=0
    colors_enabled=1
    compose_backup_interval=10
    compose_line_wrap=0
    delete_without_confirm=0
    full_header_include_local=0
    help_enabled=1
    invalid_input_notify=1
    key_back=,
    key_backward_kill_word=
    key_backward_word=
    key_begin_line=KEY_CTRLA
    key_cancel=KEY_CTRLC
    key_compose=c
    key_compose_copy=C
    key_delete=d
    key_delete_line_after_cursor=KEY_CTRLK
    key_delete_line_before_cursor=KEY_CTRLU
    key_end_line=KEY_CTRLE
    key_export=x
    key_ext_editor=KEY_CTRLW
    key_ext_html_preview=KEY_CTRLV
    key_ext_html_viewer=v
    key_ext_msg_viewer=w
    key_ext_pager=e
    key_filter_show_current_date=3
    key_filter_show_current_name=4
    key_filter_show_current_subject=5
    key_filter_show_has_attachments=2
    key_filter_show_unread=1
    key_filter_sort_reset=`
    key_find=/
    key_find_next=?
    key_forward=f
    key_forward_attached=F
    key_forward_word=
    key_goto_folder=g
    key_goto_inbox=i
    key_import=z
    key_jump_to=j
    key_kill_word=
    key_move=m
    key_next_msg=n
    key_next_page=KEY_NPAGE
    key_next_page_compose=KEY_NPAGE
    key_open=.
    key_othercmd_help=o
    key_postpone=KEY_CTRLO
    key_prev_msg=p
    key_prev_page=KEY_PPAGE
    key_prev_page_compose=KEY_PPAGE
    key_quit=q
    key_refresh=l
    key_reply_all=r
    key_reply_sender=R
    key_rich_header=KEY_CTRLR
    key_save_file=s
    key_search=/
    key_search_current_name=-
    key_search_current_subject==
    key_search_show_folder=\
    key_select_all=a
    key_select_item=KEY_SPACE
    key_send=KEY_CTRLX
    key_sort_date=#
    key_sort_has_attachments=@
    key_sort_name=$
    key_sort_subject=%
    key_sort_unread=!
    key_spell=KEY_CTRLS
    key_sync=s
    key_to_select=KEY_CTRLT
    key_toggle_full_header=h
    key_toggle_markdown_compose=KEY_CTRLN
    key_toggle_text_html=t
    key_toggle_unread=u
    localized_subject_prefixes=
    markdown_html_compose=0
    new_msg_bell=1
    persist_file_selection_dir=1
    persist_find_query=0
    persist_folder_filter=1
    persist_search_query=0
    persist_selection_on_sortfilter_change=1
    persist_sortfilter=1
    plain_text=1
    postpone_without_confirm=0
    quit_without_confirm=1
    respect_format_flowed=1
    rewrap_quoted_lines=1
    search_show_folder=0
    send_without_confirm=0
    show_embedded_images=1
    show_progress=1
    show_rich_header=0
    signature=0
    tab_size=8
    terminal_title=
    top_bar_show_version=0
    unread_indicator=N

### attachment_indicator

Controls which character to indicate that an email has attachments
(default: `📎`). For a more plain layout one can use an ascii character: `+`.

### bottom_reply

Controls whether to reply at the bottom of emails (default disabled).

### cancel_without_confirm

Allow cancelling email compose without confirmation prompt (default disabled).

### colors_enabled

Enable terminal color output (default enabled).

### compose_backup_interval

Specify interval in seconds for local backups during compose (default 10).
If the system running falanet is unexpectedly shutdown while user is composing
an email, then upon next falanet startup any backuped compose message will be
automatically uploaded to the draft folder.
Setting this parameter to 0 disables local backups.

### compose_line_wrap

Specify how falanet shall wrap lines in outgoing emails. Supported options:

    0 = none (default)
    1 = using format=flowed
    2 = hard wrap at 72 chars width

### delete_without_confirm

Allow deleting emails (moving to trash folder) without confirmation
prompt (default disabled).

### full_header_include_local

While viewing full headers (by pressing `h`) falanet displays RFC 822 headers
by default. This parameter allows enabling falanet to also display local /
internal header fields, such as server timestamp. Default disabled.

### help_enabled

Show supported keyboard shortcuts at bottom of screen (default enabled).

### invalid_input_notify

Notify user when unsupported keyboard shortcuts are input (default enabled).

### key_

Keyboard bindings for various functions (default see above). The keyboard
bindings may be specified in the following formats:
- Ncurses macro (ex: `KEY_CTRLK`)
- Hex key code (ex: `0x22e`)
- Octal key code sequence (ex: `\033\177`)
- Plain-text lower-case ASCII (ex: `r`)
- Disable key binding (`KEY_NONE`)

To determine the key code sequence for a key, one can run falanet in key code
dump mode `falanet -k` which will output the octal code, and ncurses macro name
(if present).

### localized_subject_prefixes

Email subjects are normalized (stripped of `re:`, `fwd:`) when sorting emails
by subject, and when replying to, or forwarding an email. By default only the
English prefixes `re` and `fwd?` (regex for `fwd` and `fw`) are removed. This
parameter allows extending the removal to other localized prefixes. Example
configuration for a Swedish user:

    localized_subject_prefixes=sv,vb

For a French user:

    localized_subject_prefixes=ref,tr

For a German user:

    localized_subject_prefixes=aw,wg

### markdown_html_compose

Default value for each new email, whether falanet shall enable markdown HTML
compose. I.e. whether falanet shall generate a text/html message part based on
processing the composed message as Markdown, when sending sending emails from
falanet. This can be overridden on a per-email basis by pressing CTRL-N when
editing an email (default disabled).

### new_msg_bell

Indicate new messages with terminal bell (default enabled).

### persist_file_selection_dir

Determines whether file selection view shall remember previous directory
(default enabled).

### persist_find_query

Controls whether to start with previous find query when performing repeated
find queries (default disabled).

### persist_folder_filter

Determines whether to persist move-to-folder list filter (default enabled).

### persist_search_query

Controls whether to start with previous search query when performing repeated
search queries (default disabled).

### persist_selection_on_sortfilter_change

Determines whether to keep current message list selection when
filtering/sorting mode is changed (default enabled).

### persist_sortfilter

Specifies whether each folder listing shall persist its filtering/sorting
mode (default enabled).

### plain_text

Determines whether showing plain text (vs. html converted to text) is
preferred. If the preferred email part is not present, falanet automatically
attempts to show the other. This option can be re-configured at run-time
by pressing `t` when viewing an email (default enabled).

### postpone_without_confirm

Allow postponing email compose without confirmation prompt (default disabled).

### quit_without_confirm

Allow exiting falanet without confirmation prompt (default enabled).

### respect_format_flowed

Specify whether falanet shall respect email line wrapping of format=flowed
type (default enabled).

### rewrap_quoted_lines

Control whether falanet shall rewrap quoted lines (default enabled).

### search_show_folder

Determines whether folder name should be shown in search results. This option
can be re-configured at run-time by pressing `\` when viewing search results
(default disabled).

### send_without_confirm

Allow sending email during compose without confirmation prompt (default
disabled).

### show_embedded_images

Determines whether to show embedded images in text/html part when viewing it
using external viewer; press right arrow when viewing a message to go to parts
view, and then select the text/html part and press right arrow again (default
enabled).

### show_progress

Specify how falanet shall show progress indication when fetching or indexing
emails. Supported options:

    0 = disabled
    1 = show floating point percentage (default)
    2 = show integer percentage

### show_rich_header

Determines whether to show rich headers (bcc field) during email compose. This
option can be re-configured in run-time by pressing `CTRL-R` when composing
an email (default disabled).

### signature

Determines whether to suffix emails with a signature (default disabled). When
enabled, falanet will use `~/.config/falanet/signature.txt` if present, or
otherwise use `~/.signature` for signature plain text content. When composing
markdown formatted emails, falanet will use `~/.config/falanet/signature.html` if
present, for the html part, and otherwise simply convert the plain text
signature to html.

Note: For **custom html** signature to work properly, the plain text signature
should not be present more than once in the composed message, thus a very short
plain text signature may not be ideal.

Example signature files: [signature.txt](/doc/signature.txt),
[signature.html](/doc/signature.html)

### tab_size

Tabs are expanded to spaces when viewed in falanet. This parameter controls the
space between tab stops (default 8).

### terminal_title

Specifies custom terminal title, ex: `terminal_title=falanet - d99kris@email.com`.

### unread_indicator

Controls which character to indicate that an email is unread (default: `N`).
For a more graphical interface, an emoji such as `✉` can be used.


~/.config/falanet/colors.conf
---------------------------
This configuration file controls the configurable colors of falanet. For this
configuration to take effect, `colors_enabled=1` must be set in
`~/.config/falanet/ui.conf`.

Example color config files are provided in `/usr/local/share/falanet/themes`
and can be used by overwriting `~/.config/falanet/colors.conf`.

### Htop style theme

This color theme is similar to htop's default, see screenshot below with
falanet and htop.

![screenshot falanet htop style theme](/doc/screenshot-falanet-htop-theme.png)

To use this config:

    cp /usr/local/share/falanet/themes/htop-style.conf ~/.config/falanet/colors.conf

### Manual configuration

Alternatively one may manually edit `colors.conf`. Colors may
be specified using standard palette names (`black`, `red`, `green`, `yellow`,
`blue`, `magenta`, `cyan`, `white`, `gray`, `bright_red`, `bright_green`,
`bright_yellow`, `bright_blue`, `bright_magenta`, `bright_cyan` and
`bright_white`) or using integer palette numbers (`0`, `1`, `2`, etc).

To use default terminal color, leave the color empty or set it to `normal`.
To use inverted / reverse color set both `fg` and `bg` values to `reverse`.

For terminals supporting custom palettes it is also possible to specify colors
using six digit hex format with `0x` prefix, e.g. `0xa0a0a0`. For each item
background `_bg` and foreground `_fg` can be specified. Default
configuration file:

    color_dialog_bg=reverse
    color_dialog_fg=reverse
    color_help_desc_bg=
    color_help_desc_fg=
    color_help_keys_bg=reverse
    color_help_keys_fg=reverse
    color_highlighted_text_bg=reverse
    color_highlighted_text_fg=reverse
    color_quoted_text_bg=
    color_quoted_text_fg=gray
    color_regular_text_bg=
    color_regular_text_fg=
    color_selected_item_bg=
    color_selected_item_fg=gray
    color_top_bar_bg=reverse
    color_top_bar_fg=reverse

### color_dialog

User prompt dialogs and notifications at bottom of the screen, just above the
help bar.

### color_help_desc

Help shortcut description texts at bottom of the screen, i.e. `Compose` in
`C Compose`.

### color_help_keys

Help shortcut key binding texts at bottom of the screen, i.e. `C` in
`C Compose`.

### color_highlighted_text

Highlighted text, such as current message in message view, current folder in
folder list, text strings found in message find, etc.

### color_quoted_text

Quoted message text (lines starting with `>`).

### color_regular_text

Default text color.

### color_selected_item

Selected messages in message list view.

### color_top_bar

Top / title bar.

Accessing Email Cache using Other Email Clients
===============================================

The falanet message cache may be exported to the Maildir format using the
following command:

    falanet --export ~/Maildir

A basic `~/.muttrc` config file for reading the exported Maildir in `mutt`:

    set mbox_type=Maildir
    set spoolfile="~/Maildir"
    set folder="~/Maildir"
    set mask=".*"

Note: falanet is not designed for working with other email clients, this export
option is mainly available as a data recovery option in case access to an
email account is lost, and one needs a local Maildir archive to import into
a new email account. Such import is not supported by falanet, but is supported
by some other email clients, like Thunderbird.


Technical Details
=================

Third-party Libraries
---------------------
falanet is implemented in C++. Its source tree includes the source code from the
following third-party libraries:

- [apathy](https://github.com/dlecocq/apathy) -
  Copyright 2013 Dan Lecocq - [MIT License](/ext/apathy/LICENSE)
- [cereal](https://github.com/USCiLab/cereal) -
  Copyright 2014 Randolph Voorhies, Shane Grant - [BSD-3 License](/ext/cereal/LICENSE)
- [cxx-prettyprint](https://github.com/louisdx/cxx-prettyprint) -
  Copyright 2010 Louis Delacroix - [Boost License](/ext/cxx-prettyprint/LICENSE_1_0.txt)
- [cyrus-imap](https://opensource.apple.com/source/CyrusIMAP/CyrusIMAP-156.9/cyrus_imap) -
  Copyright 1994-2000 Carnegie Mellon University - [BSD-3 License](/ext/cyrus-imap/COPYRIGHT)
- [libetpan](https://github.com/dinhvh/libetpan) -
  Copyright 2001-2005 Dinh Viet Hoa - [BSD-3 License](/ext/libetpan/COPYRIGHT)
- [sqlite_modern_cpp](https://github.com/SqliteModernCpp/sqlite_modern_cpp) -
  Copyright 2017 aminroosta - [MIT License](/ext/sqlite_modern_cpp/License.txt)

Code Formatting
---------------
Uncrustify is used to maintain consistent source code formatting, example:

    uncrustify -c etc/uncrustify.cfg --replace --no-backup src/*.cpp src/*.h


License
=======

falanet is distributed under the MIT license. See [LICENSE](/LICENSE) file.
