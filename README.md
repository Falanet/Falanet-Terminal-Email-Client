# Falaclient

A beautiful, fast, and feature-rich terminal-based email client designed specifically for the Falanet mail service with modern UI enhancements.

![License](https://img.shields.io/badge/license-MIT-blue.svg)
![Platform](https://img.shields.io/badge/platform-Linux%20|%20macOS-lightgrey.svg)
![Language](https://img.shields.io/badge/language-C%2B%2B-blue.svg)

## Features

### Beautiful Modern Interface
- **Enhanced UI** with stunning colors and Unicode icons
- **Folder-specific icons** (Inbox, Sent, Trash, etc.)
- **Color-coded message states** (unread, selected, active)
- **Beautiful status bars** and progress indicators
- **Syntax highlighting** for email headers, quotes, and URLs

### Core Email Features
- **Falanet IMAP & SMTP** integration
- **HTML email rendering** with enhanced terminal display
- **Attachment handling** with visual indicators
- **Email search** with highlighting
- **Draft messages** support
- **Address book** auto-generated from contacts
- **Offline composition** capabilities

### Advanced Features
- **Multi-threaded** background operations
- **Local caching** with SQLite (optional AES256 encryption)
- **External editor** integration ($EDITOR)
- **External viewer** support ($PAGER)
- **Markdown to HTML** email composition
- **Custom signatures**
- **Configurable themes** and colors

### Quick Setup
- **Simple configuration** files
- **Intuitive keybindings** similar to Alpine/Pine
- **Optimized for Falanet** mail service

## Installation

### Quick Install (Recommended)

```bash
# Clone the repository
git clone https://github.com/Falanet/falaclient.git
cd falaclient

# Build with CMake (one-step process)
mkdir build && cd build
cmake ..
make -j$(nproc)
sudo make install
```

### Advanced Build Options

```bash
# Install system dependencies automatically
cmake -DFALACLIENT_INSTALL_DEPS=ON ..

# Debug build
cmake -DCMAKE_BUILD_TYPE=Debug ..

# Format source code
cmake -DFALACLIENT_FORMAT_CODE=ON ..

# Build documentation
cmake -DFALACLIENT_BUILD_DOC=ON ..

# Bump version
cmake -DFALACLIENT_BUMP_VERSION=ON ..

# Combined example
cmake -DCMAKE_BUILD_TYPE=Debug -DFALACLIENT_INSTALL_DEPS=ON -DFALACLIENT_BUILD_DOC=ON ..
```

### Manual Build

<details>
<summary>Click to expand manual build instructions</summary>

#### Dependencies

**Ubuntu/Debian:**
```bash
sudo apt install git cmake build-essential libssl-dev libreadline-dev \
    libncurses5-dev libxapian-dev libsqlite3-dev libsasl2-dev \
    libsasl2-modules libcurl4-openssl-dev libexpat-dev zlib1g-dev \
    libmagic-dev uuid-dev
```

**Fedora:**
```bash
sudo yum install cmake openssl-devel ncurses-devel xapian-core-devel \
    sqlite-devel cyrus-sasl-devel cyrus-sasl-plain libcurl-devel \
    expat-devel zlib-devel file-devel libuuid-devel clang
```

**macOS:**
```bash
brew install openssl ncurses xapian sqlite libmagic ossp-uuid
```

**Arch Linux:**
```bash
sudo pacman -Sy cmake make openssl ncurses xapian-core sqlite \
    cyrus-sasl curl expat zlib file
```

**OpenBSD:**
```bash
doas pkg_add cmake curl sqlite3 xapian-core libmagic curl expat cyrus-sasl ossp-uuid libiconv
```

#### Build
```bash
mkdir -p build && cd build
cmake ..
make -j$(nproc)
sudo make install
```

**OpenBSD Build Notes:**

```bash
# First, create the UUID header symlink for ossp-uuid compatibility:
doas mkdir -p /usr/local/include/uuid
doas ln -s /usr/local/include/ossp/uuid.h /usr/local/include/uuid/uuid.h

# Then build with specific library paths:
cmake .. \
  -DCURSES_INCLUDE_PATH=/usr/include \
  -DCURSES_LIBRARY=/usr/lib/libcurses.so \
  -DUUID_INCLUDE_DIR=/usr/local/include \
  -DUUID_LIBRARY=/usr/local/lib/libuuid.so \
  -DICONV_INCLUDE_DIR=/usr/local/include \
  -DICONV_LIBRARY=/usr/local/lib/libiconv.so
```

</details>

## Quick Start

### First Run Setup
```bash
# Start Falaclient
falaclient

# Configure your falanet email account
# Edit ~/.config/falaclient/main.conf with your credentials:
# - address=your-username@falanet.org
# - imap_host=mail.falanet.org
# - smtp_host=mail.falanet.org
# - user=your-username@falanet.org
```

### Command Line Options
```bash
falaclient [OPTIONS]

-d, --confdir <DIR>     Use different config directory
-e, --verbose           Enable verbose logging  
-ee, --extra-verbose    Enable extra verbose logging
-h, --help              Show help message
-k, --keydump           Key code dump mode
-o, --offline           Run in offline mode
-p, --pass              Change password
-v, --version           Show version information
-x, --export <DIR>      Export cache to Maildir format
```

## Customization

### Theme Configuration
Edit `~/.config/falaclient/themes/default.conf`:

```ini
# Beautiful Modern Theme
color_dialog_bg=blue
color_dialog_fg=white
color_quoted_text_fg=blue
color_highlighted_text_bg=yellow
color_highlighted_text_fg=black
color_top_bar_bg=blue
color_top_bar_fg=white

# Enhanced email-specific colors
color_email_header_fg=green
color_email_subject_fg=yellow
color_attachment_indicator_fg=magenta
color_unread_indicator_bg=yellow
color_sender_name_fg=white
color_date_stamp_fg=green
```

### Key Bindings
- `↑/↓` - Navigate messages
- `Enter` - Read message
- `c` - Compose new message
- `r` - Reply to message
- `f` - Forward message
- `d` - Delete message
- `g` - Go to folder
- `s` - Search messages
- `/` - Find in message
- `q` - Quit

## Configuration Files

```
~/.config/falaclient/
├── main.conf           # Main account settings
├── ui.conf            # UI customization
└── themes/
    └── default.conf   # Color theme
```

### Setting up your Falaclient Account

First, copy the default configuration file to your user config directory:

```bash
# Create config directory if it doesn't exist
mkdir -p ~/.config/falaclient

# Copy the default config file
cp /usr/local/share/falaclient/main.conf ~/.config/falaclient/main.conf
```

Then open the config file `~/.config/falaclient/main.conf` in your favorite text editor and configure your Falaclient account:

```ini
# Falaclient Email Configuration
address=your-username@falanet.org
drafts=Drafts
imap_host=mail.falanet.org
imap_port=993
inbox=Inbox
name=Your Full Name
sent=Sent
smtp_host=mail.falanet.org
smtp_port=587
trash=Trash
user=your-username@falanet.org
```

Replace `your-username` with your actual Falaclient username and `Your Full Name` with your display name.

## Enhanced Features

### Beautiful UI Elements
- **Unicode Icons**: Visual indicators for different message types
- **Color Coding**: Unread (yellow), selected (cyan), active (blue)
- **Smart Folders**: Automatic icons based on folder type
- **Status Indicators**: Real-time connection and sync status
- **Progress Bars**: Visual feedback for operations

### HTML Email Support
- **Enhanced Parser**: Better HTML-to-text conversion
- **Link Detection**: Highlighted URLs in messages
- **Table Support**: Improved table rendering in terminal
- **Image Placeholders**: Indicators for embedded images

### Advanced Search
- **Full-text Search**: Search across all message content
- **Field-specific Search**: Search by sender, subject, date
- **Search Highlighting**: Results highlighted in message view
- **Quick Filters**: Unread, attachments, date-based filters

## Troubleshooting

<details>
<summary>Common Issues and Solutions</summary>

### Connection Issues
```bash
# Test IMAP connection
falaclient --verbose

# Check SSL/TLS settings
# Edit ~/.config/falaclient/main.conf
imap_server_port=993
imap_server_ssl=true
```

### Performance Issues
```bash
# Clear cache
rm -rf ~/.cache/falaclient/

# Reduce prefetch level
# Edit ~/.config/falaclient/main.conf
prefetch_level=1
```

### Display Issues
```bash
# Check terminal capabilities
echo $TERM
infocmp

# Enable/disable colors
# Edit ~/.config/falaclient/ui.conf
colors_enabled=true
```

</details>

## Contributing

We welcome contributions! Here's how you can help:

1. **Fork** the repository
2. **Create** a feature branch (`git checkout -b feature/amazing-feature`)
3. **Commit** your changes (`git commit -m 'Add amazing feature'`)
4. **Push** to the branch (`git push origin feature/amazing-feature`)
5. **Open** a Pull Request

## System Requirements

- **OS**: Linux (Ubuntu 18.04+), macOS (10.14+)
- **CPU**: x86_64, ARM64
- **RAM**: 512MB minimum, 1GB recommended
- **Terminal**: xterm, gnome-terminal, iTerm2, or compatible
- **Network**: Internet connection for email sync

## Security Features

- **AES256 Encryption**: Optional local cache encryption
- **TLS/SSL**: Encrypted connections to Falanet email servers
- **Secure Authentication**: Direct integration with Falanet mail service
- **No Password Storage**: Secure credential handling
- **Local Cache**: All data stored locally

## License

This project is licensed under the MIT License - see the [LICENSE](LICENSE) file for details.

## Acknowledgments

- Based on the original nmail project
- Enhanced UI inspired by modern terminal applications
- Community feedback and contributions
- Email protocol implementations using libetpan

---

<div align="center">

**Star this repository if you find it useful!**

</div>

## Usage Guide

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

The built-in email compose editor in falaclient supports the following:

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

falaclient caches data locally to improve performance. Cached data can be encrypted
by setting by setting `cache_encrypt=1` in main.conf. Message databases are
then encrypted using OpenSSL AES256-CBC with a key derived from a random salt
and the email account password. Folder names are hashed using SHA256 (thus not
encrypted).

Storing the account password (`save_pass=1` in main.conf) is *not* secure.
While falaclient encrypts the password, the key is trivial to determine from
the source code. Only store the password if measurements are taken to ensure
`~/.config/falaclient/secret.conf` cannot by accessed by a third-party.


Configuration
=============

Aside from `main.conf` covered above, the following files can be used to
configure falaclient.

~/.config/falaclient/ui.conf
-----------------------
This configuration file controls the UI aspects of falaclient. Default configuration
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
(default: `+`). For a more plain layout one can use an ascii character: `+`.

### bottom_reply

Controls whether to reply at the bottom of emails (default disabled).

### cancel_without_confirm

Allow cancelling email compose without confirmation prompt (default disabled).

### colors_enabled

Enable terminal color output (default enabled).

### compose_backup_interval

Specify interval in seconds for local backups during compose (default 10).
If the system running falaclient is unexpectedly shutdown while user is composing
an email, then upon next falaclient startup any backuped compose message will be
automatically uploaded to the draft folder.
Setting this parameter to 0 disables local backups.

### compose_line_wrap

Specify how falaclient shall wrap lines in outgoing emails. Supported options:

    0 = none (default)
    1 = using format=flowed
    2 = hard wrap at 72 chars width

### delete_without_confirm

Allow deleting emails (moving to trash folder) without confirmation
prompt (default disabled).

### full_header_include_local

While viewing full headers (by pressing `h`) falaclient displays RFC 822 headers
by default. This parameter allows enabling falaclient to also display local /
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

To determine the key code sequence for a key, one can run falaclient in key code
dump mode `falaclient -k` which will output the octal code, and ncurses macro name
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

Default value for each new email, whether falaclient shall enable markdown HTML
compose. I.e. whether falaclient shall generate a text/html message part based on
processing the composed message as Markdown, when sending sending emails from
falaclient. This can be overridden on a per-email basis by pressing CTRL-N when
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
preferred. If the preferred email part is not present, falaclient automatically
attempts to show the other. This option can be re-configured at run-time
by pressing `t` when viewing an email (default enabled).

### postpone_without_confirm

Allow postponing email compose without confirmation prompt (default disabled).

### quit_without_confirm

Allow exiting falaclient without confirmation prompt (default enabled).

### respect_format_flowed

Specify whether falaclient shall respect email line wrapping of format=flowed
type (default enabled).

### rewrap_quoted_lines

Control whether falaclient shall rewrap quoted lines (default enabled).

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

Specify how falaclient shall show progress indication when fetching or indexing
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
enabled, falaclient will use `~/.config/falaclient/signature.txt` if present, or
otherwise use `~/.signature` for signature plain text content. When composing
markdown formatted emails, falaclient will use `~/.config/falaclient/signature.html` if
present, for the html part, and otherwise simply convert the plain text
signature to html.

Note: For **custom html** signature to work properly, the plain text signature
should not be present more than once in the composed message, thus a very short
plain text signature may not be ideal.

Example signature files: [signature.txt](/doc/signature.txt),
[signature.html](/doc/signature.html)

### tab_size

Tabs are expanded to spaces when viewed in falaclient. This parameter controls the
space between tab stops (default 8).

### terminal_title

Specifies custom terminal title, ex: `terminal_title=falaclient - d99kris@email.com`.

### unread_indicator

Controls which character to indicate that an email is unread (default: `N`).
For a more graphical interface, a character such as `*` can be used.


~/.config/falaclient/colors.conf
---------------------------
This configuration file controls the configurable colors of falaclient. For this
configuration to take effect, `colors_enabled=1` must be set in
`~/.config/falaclient/ui.conf`.

Example color config files are provided in `/usr/local/share/falaclient/themes`
and can be used by overwriting `~/.config/falaclient/colors.conf`.

### Htop style theme

This color theme is similar to htop's default, see screenshot below with
falaclient and htop.

![screenshot falaclient htop style theme](/doc/screenshot-falaclient-htop-theme.png)

To use this config:

    cp /usr/local/share/falaclient/themes/htop-style.conf ~/.config/falaclient/colors.conf

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

The falaclient message cache may be exported to the Maildir format using the
following command:

    falaclient --export ~/Maildir

A basic `~/.muttrc` config file for reading the exported Maildir in `mutt`:

    set mbox_type=Maildir
    set spoolfile="~/Maildir"
    set folder="~/Maildir"
    set mask=".*"

Note: falaclient is not designed for working with other email clients, this export
option is mainly available as a data recovery option in case access to an
email account is lost, and one needs a local Maildir archive to import into
a new email account. Such import is not supported by falaclient, but is supported
by some other email clients, like Thunderbird.


Technical Details
=================

Third-party Libraries
---------------------
falaclient is implemented in C++. Its source tree includes the source code from the
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

falaclient is distributed under the MIT license. See [LICENSE](/LICENSE) file.
