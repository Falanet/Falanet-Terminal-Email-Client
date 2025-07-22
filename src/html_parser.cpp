// enhanced_html_parser.cpp
//
// Implementation of enhanced HTML parser for beautiful terminal display

#include "html_parser.h"
#include <algorithm>
#include <sstream>
#include <iomanip>
#include <ncurses.h>

// Static member definitions
const std::map<std::string, std::string> HtmlParser::s_htmlEntities = {
  {"&amp;", "&"}, {"&lt;", "<"}, {"&gt;", ">"}, {"&quot;", "\""}, {"&apos;", "'"},
  {"&nbsp;", " "}, {"&copy;", "(c)"}, {"&reg;", "(R)"}, {"&trade;", "(TM)"},
  {"&mdash;", "--"}, {"&ndash;", "-"}, {"&hellip;", "..."}, {"&laquo;", "<<"},
  {"&raquo;", ">>"}, {"&ldquo;", "\""}, {"&rdquo;", "\""}, {"&lsquo;", "'"},
  {"&rsquo;", "'"}, {"&bull;", "*"}, {"&middot;", "."}, {"&sect;", "§"}
};

const std::map<std::string, std::string> HtmlParser::s_emojiMap = {
  {":)", "🙂"}, {":(", "🙁"}, {":D", "😀"}, {":P", "😛"}, {";)", "😉"},
  {"<3", "❤"}, {"</3", "💔"}, {":heart:", "❤"}, {":star:", "⭐"},
  {":check:", "✅"}, {":x:", "❌"}, {":warning:", "⚠"}, {":info:", "ℹ"},
  {":mail:", "📧"}, {":link:", "🔗"}, {":file:", "📄"}, {":image:", "🖼"}
};

const std::map<std::string, bool> HtmlParser::s_boldTags = {
  {"b", true}, {"strong", true}, {"h1", true}, {"h2", true}, 
  {"h3", true}, {"h4", true}, {"h5", true}, {"h6", true}
};

const std::map<std::string, bool> HtmlParser::s_italicTags = {
  {"i", true}, {"em", true}, {"cite", true}, {"var", true}
};

const std::map<std::string, bool> HtmlParser::s_underlineTags = {
  {"u", true}, {"ins", true}
};

const std::map<std::string, int> HtmlParser::s_defaultColors = {
  {"h1", COLOR_BLUE}, {"h2", COLOR_BLUE}, {"h3", COLOR_BLUE},
  {"h4", COLOR_CYAN}, {"h5", COLOR_CYAN}, {"h6", COLOR_CYAN},
  {"a", COLOR_CYAN}, {"code", COLOR_GREEN}, {"pre", COLOR_GREEN},
  {"blockquote", COLOR_YELLOW}, {"em", COLOR_MAGENTA}, {"strong", COLOR_RED}
};

HtmlParser::HtmlParser()
{
  TerminalColors::InitializeColors();
}

HtmlParser::~HtmlParser() = default;

std::vector<TerminalFormat> HtmlParser::ParseHtmlToTerminal(const std::string& htmlContent)
{
  m_extractedLinks.clear();
  
  // Clean up HTML - remove scripts, styles, meta tags
  std::string cleanHtml = htmlContent;
  
  // Remove script tags and content
  std::regex scriptRegex(R"(<script[^>]*>[\s\S]*?</script>)", std::regex_constants::icase);
  cleanHtml = std::regex_replace(cleanHtml, scriptRegex, "");
  
  // Remove style tags and content
  std::regex styleRegex(R"(<style[^>]*>[\s\S]*?</style>)", std::regex_constants::icase);
  cleanHtml = std::regex_replace(cleanHtml, styleRegex, "");
  
  // Remove meta, link, and other head elements
  std::regex headElementsRegex(R"(<(?:meta|link|title)[^>]*>)", std::regex_constants::icase);
  cleanHtml = std::regex_replace(cleanHtml, headElementsRegex, "");
  
  // Parse the cleaned HTML
  size_t position = 0;
  std::vector<TerminalFormat> result;
  
  while (position < cleanHtml.length()) {
    if (cleanHtml[position] == '<') {
      HtmlElement element = ParseElement(cleanHtml, position);
      std::vector<TerminalFormat> elementFormat = ConvertElementToTerminal(element);
      result.insert(result.end(), elementFormat.begin(), elementFormat.end());
    } else {
      // Handle text content outside of tags
      std::string textContent;
      while (position < cleanHtml.length() && cleanHtml[position] != '<') {
        textContent += cleanHtml[position];
        position++;
      }
      
      if (!textContent.empty()) {
        TerminalFormat format;
        format.text = ProcessTextContent(textContent, HtmlElement{});
        result.push_back(format);
      }
    }
  }
  
  return result;
}

HtmlElement HtmlParser::ParseElement(const std::string& html, size_t& position)
{
  HtmlElement element;
  
  if (position >= html.length() || html[position] != '<') {
    return element;
  }
  
  // Find the end of the opening tag
  size_t tagEnd = html.find('>', position);
  if (tagEnd == std::string::npos) {
    position = html.length();
    return element;
  }
  
  std::string tagString = html.substr(position + 1, tagEnd - position - 1);
  element.tag = ExtractTagName(tagString);
  element.attributes = ParseAttributes(tagString);
  
  position = tagEnd + 1;
  
  // Check for self-closing tags
  if (tagString.back() == '/' || 
      element.tag == "br" || element.tag == "hr" || element.tag == "img" || 
      element.tag == "input" || element.tag == "meta" || element.tag == "link") {
    return element;
  }
  
  // Parse content until closing tag
  std::string closingTag = "</" + element.tag + ">";
  size_t contentStart = position;
  size_t closingTagPos = html.find(closingTag, position);
  
  if (closingTagPos == std::string::npos) {
    // No closing tag found, take remaining content
    element.content = html.substr(contentStart);
    position = html.length();
  } else {
    element.content = html.substr(contentStart, closingTagPos - contentStart);
    position = closingTagPos + closingTag.length();
  }
  
  return element;
}

std::map<std::string, std::string> HtmlParser::ParseAttributes(const std::string& attrString)
{
  std::map<std::string, std::string> attributes;
  
  std::regex attrRegex(R"((\w+)=["']([^"']*?)["'])");
  std::sregex_iterator iter(attrString.begin(), attrString.end(), attrRegex);
  std::sregex_iterator end;
  
  for (; iter != end; ++iter) {
    std::smatch match = *iter;
    attributes[ToLower(match[1].str())] = match[2].str();
  }
  
  return attributes;
}

std::string HtmlParser::ExtractTagName(const std::string& tagString)
{
  size_t spacePos = tagString.find(' ');
  size_t slashPos = tagString.find('/');
  size_t endPos = std::min(spacePos, slashPos);
  
  if (endPos == std::string::npos) {
    endPos = tagString.length();
  }
  
  return ToLower(tagString.substr(0, endPos));
}

std::vector<TerminalFormat> HtmlParser::ConvertElementToTerminal(const HtmlElement& element, int depth)
{
  std::vector<TerminalFormat> result;
  
  // Handle special elements
  if (element.tag == "br") {
    TerminalFormat format;
    format.text = "\n";
    result.push_back(format);
    return result;
  }
  
  if (element.tag == "hr") {
    TerminalFormat format;
    format.text = std::string(m_terminalWidth - depth * m_indentSize, '-') + "\n";
    format.colorPair = TerminalColors::CreateColorPair(COLOR_CYAN, -1);
    result.push_back(format);
    return result;
  }
  
  if (element.tag == "table" && m_renderTables) {
    return RenderTable(element);
  }
  
  if ((element.tag == "ul" || element.tag == "ol") && depth < 3) {
    return RenderList(element, element.tag == "ol");
  }
  
  // Handle block elements with newlines
  bool isBlockElement = (element.tag == "p" || element.tag == "div" || 
                        element.tag.substr(0, 1) == "h" || element.tag == "blockquote" ||
                        element.tag == "pre" || element.tag == "address");
  
  if (isBlockElement && !result.empty()) {
    TerminalFormat newline;
    newline.text = "\n";
    result.push_back(newline);
  }
  
  // Process content
  std::string processedContent = ProcessTextContent(element.content, element);
  
  if (!processedContent.empty()) {
    TerminalFormat format;
    format.text = processedContent;
    format.indentLevel = depth;
    format.colorPair = GetColorForElement(element.tag);
    format.isBold = ShouldBeBold(element.tag);
    format.isItalic = ShouldBeItalic(element.tag);
    format.isUnderlined = ShouldBeUnderlined(element.tag);
    
    // Handle links
    if (element.tag == "a" && m_extractLinks) {
      auto hrefIt = element.attributes.find("href");
      if (hrefIt != element.attributes.end()) {
        format.isLink = true;
        format.linkUrl = hrefIt->second;
        m_extractedLinks.push_back(hrefIt->second);
        
        // Add link indicator
        format.text += " 🔗[" + std::to_string(m_extractedLinks.size()) + "]";
      }
    }
    
    // Wrap text for proper display
    if (element.tag != "pre" && element.tag != "code") {
      format.text = WrapText(format.text, m_terminalWidth - depth * m_indentSize, depth * m_indentSize);
    }
    
    result.push_back(format);
  }
  
  // Add trailing newline for block elements
  if (isBlockElement) {
    TerminalFormat newline;
    newline.text = "\n";
    result.push_back(newline);
  }
  
  return result;
}

std::string HtmlParser::ProcessTextContent(const std::string& text, const HtmlElement& parent)
{
  std::string processed = text;
  
  // Convert HTML entities
  processed = ConvertHtmlEntities(processed);
  
  // Convert emojis if enabled
  if (m_convertEmojis) {
    processed = ConvertEmojis(processed);
  }
  
  // Normalize whitespace (except for pre elements)
  if (parent.tag != "pre" && parent.tag != "code") {
    // Replace multiple whitespace with single space
    processed = std::regex_replace(processed, std::regex(R"(\s+)"), " ");
    processed = Trim(processed);
  }
  
  return processed;
}

std::string HtmlParser::ConvertHtmlEntities(const std::string& text)
{
  std::string result = text;
  
  for (const auto& entity : s_htmlEntities) {
    size_t pos = 0;
    while ((pos = result.find(entity.first, pos)) != std::string::npos) {
      result.replace(pos, entity.first.length(), entity.second);
      pos += entity.second.length();
    }
  }
  
  // Handle numeric entities manually
  std::regex numericEntityRegex(R"(&(#?)(\d+);)");
  std::smatch match;
  std::string::size_type start = 0;
  std::string temp = result;
  result.clear();
  
  while (std::regex_search(temp.cbegin() + start, temp.cend(), match, numericEntityRegex)) {
    result += temp.substr(start, match.position());
    
    try {
      int code = std::stoi(match[2].str());
      if (code < 128) {
        result += static_cast<char>(code);
      } else {
        result += "?"; // Placeholder for unsupported characters
      }
    } catch (...) {
      result += match[0].str(); // Return original if conversion fails
    }
    
    start += match.position() + match.length();
  }
  result += temp.substr(start);
  
  return result;
}

std::string HtmlParser::ConvertEmojis(const std::string& text)
{
  std::string result = text;
  
  for (const auto& emoji : s_emojiMap) {
    size_t pos = 0;
    while ((pos = result.find(emoji.first, pos)) != std::string::npos) {
      result.replace(pos, emoji.first.length(), emoji.second);
      pos += emoji.second.length();
    }
  }
  
  return result;
}

std::string HtmlParser::WrapText(const std::string& text, int maxWidth, int indent)
{
  if (maxWidth <= indent) return text;
  
  std::string result;
  std::string indentStr(indent, ' ');
  std::istringstream words(text);
  std::string word;
  std::string currentLine = indentStr;
  
  while (words >> word) {
    if (currentLine.length() + word.length() + 1 > static_cast<size_t>(maxWidth)) {
      result += currentLine + "\n";
      currentLine = indentStr + word;
    } else {
      if (currentLine != indentStr) {
        currentLine += " ";
      }
      currentLine += word;
    }
  }
  
  if (currentLine != indentStr) {
    result += currentLine;
  }
  
  return result;
}

std::vector<TerminalFormat> HtmlParser::RenderTable(const HtmlElement& table)
{
  std::vector<TerminalFormat> result;
  std::vector<std::vector<std::string>> tableData = ExtractTableData(table);
  
  if (tableData.empty()) {
    return result;
  }
  
  // Calculate column widths
  std::vector<int> columnWidths;
  for (const auto& row : tableData) {
    for (size_t i = 0; i < row.size(); ++i) {
      if (columnWidths.size() <= i) {
        columnWidths.push_back(0);
      }
      columnWidths[i] = std::max(columnWidths[i], static_cast<int>(row[i].length()));
    }
  }
  
  // Adjust for terminal width
  int totalWidth = 0;
  for (int width : columnWidths) {
    totalWidth += width + 3; // +3 for padding and separators
  }
  
  if (totalWidth > m_terminalWidth) {
    // Proportionally reduce column widths
    double scale = static_cast<double>(m_terminalWidth - columnWidths.size() * 3) / (totalWidth - columnWidths.size() * 3);
    for (int& width : columnWidths) {
      width = std::max(5, static_cast<int>(width * scale));
    }
  }
  
  // Render table
  for (size_t i = 0; i < tableData.size(); ++i) {
    TerminalFormat rowFormat;
    rowFormat.text = FormatTableRow(tableData[i], columnWidths) + "\n";
    rowFormat.colorPair = TerminalColors::CreateColorPair(COLOR_CYAN, -1);
    
    if (i == 0) {
      rowFormat.isBold = true;
    }
    
    result.push_back(rowFormat);
    
    // Add separator after header
    if (i == 0) {
      TerminalFormat separator;
      separator.text = std::string(m_terminalWidth, '-') + "\n";
      separator.colorPair = TerminalColors::CreateColorPair(COLOR_BLUE, -1);
      result.push_back(separator);
    }
  }
  
  return result;
}

std::vector<std::vector<std::string>> HtmlParser::ExtractTableData(const HtmlElement& table)
{
  std::vector<std::vector<std::string>> result;
  
  // Simple table parsing - look for tr elements in content
  std::regex trRegex(R"(<tr[^>]*>(.*?)</tr>)", std::regex_constants::icase);
  std::sregex_iterator trIter(table.content.begin(), table.content.end(), trRegex);
  std::sregex_iterator end;
  
  for (; trIter != end; ++trIter) {
    std::smatch trMatch = *trIter;
    std::string rowContent = trMatch[1].str();
    
    std::vector<std::string> row;
    std::regex tdRegex(R"(<t[hd][^>]*>(.*?)</t[hd]>)", std::regex_constants::icase);
    std::sregex_iterator tdIter(rowContent.begin(), rowContent.end(), tdRegex);
    std::sregex_iterator tdEnd;
    
    for (; tdIter != tdEnd; ++tdIter) {
      std::smatch tdMatch = *tdIter;
      std::string cellContent = tdMatch[1].str();
      
      // Strip HTML tags from cell content
      cellContent = std::regex_replace(cellContent, std::regex(R"(<[^>]*>)"), "");
      cellContent = ConvertHtmlEntities(cellContent);
      cellContent = Trim(cellContent);
      
      row.push_back(cellContent);
    }
    
    if (!row.empty()) {
      result.push_back(row);
    }
  }
  
  return result;
}

std::string HtmlParser::FormatTableRow(const std::vector<std::string>& row, const std::vector<int>& columnWidths)
{
  std::string result = "│";
  
  for (size_t i = 0; i < row.size() && i < columnWidths.size(); ++i) {
    std::string cell = row[i];
    if (static_cast<int>(cell.length()) > columnWidths[i]) {
      cell = cell.substr(0, columnWidths[i] - 3) + "...";
    }
    
    result += " " + cell;
    result += std::string(columnWidths[i] - cell.length() + 1, ' ');
    result += "│";
  }
  
  return result;
}

std::vector<TerminalFormat> HtmlParser::RenderList(const HtmlElement& list, bool isOrdered)
{
  std::vector<TerminalFormat> result;
  
  std::regex liRegex(R"(<li[^>]*>(.*?)</li>)", std::regex_constants::icase);
  std::sregex_iterator liIter(list.content.begin(), list.content.end(), liRegex);
  std::sregex_iterator end;
  
  int counter = 1;
  for (; liIter != end; ++liIter) {
    std::smatch liMatch = *liIter;
    std::string itemContent = liMatch[1].str();
    
    TerminalFormat format;
    if (isOrdered) {
      format.text = "  " + std::to_string(counter) + ". ";
    } else {
      format.text = "  • ";
    }
    
    // Strip HTML and process content
    itemContent = std::regex_replace(itemContent, std::regex(R"(<[^>]*>)"), "");
    itemContent = ConvertHtmlEntities(itemContent);
    itemContent = Trim(itemContent);
    
    format.text += itemContent + "\n";
    result.push_back(format);
    
    counter++;
  }
  
  return result;
}

int HtmlParser::GetColorForElement(const std::string& tag)
{
  auto it = s_defaultColors.find(tag);
  if (it != s_defaultColors.end()) {
    return TerminalColors::CreateColorPair(it->second, -1);
  }
  
  auto schemeIt = m_colorScheme.find(tag);
  if (schemeIt != m_colorScheme.end()) {
    return schemeIt->second;
  }
  
  return -1; // Default color
}

bool HtmlParser::ShouldBeBold(const std::string& tag)
{
  return s_boldTags.find(tag) != s_boldTags.end();
}

bool HtmlParser::ShouldBeItalic(const std::string& tag)
{
  return s_italicTags.find(tag) != s_italicTags.end();
}

bool HtmlParser::ShouldBeUnderlined(const std::string& tag)
{
  return s_underlineTags.find(tag) != s_underlineTags.end();
}

std::string HtmlParser::Trim(const std::string& str)
{
  size_t start = str.find_first_not_of(" \t\n\r");
  if (start == std::string::npos) {
    return "";
  }
  
  size_t end = str.find_last_not_of(" \t\n\r");
  return str.substr(start, end - start + 1);
}

std::string HtmlParser::ToLower(const std::string& str)
{
  std::string result = str;
  std::transform(result.begin(), result.end(), result.begin(), ::tolower);
  return result;
}

bool HtmlParser::IsWhitespace(char c)
{
  return c == ' ' || c == '\t' || c == '\n' || c == '\r';
}

// TerminalColors implementation
std::map<std::string, int> TerminalColors::s_colorMap;
int TerminalColors::s_nextColorPair = 1;

void TerminalColors::InitializeColors()
{
  if (has_colors()) {
    start_color();
    use_default_colors();
    
    s_colorMap["black"] = COLOR_BLACK;
    s_colorMap["red"] = COLOR_RED;
    s_colorMap["green"] = COLOR_GREEN;
    s_colorMap["yellow"] = COLOR_YELLOW;
    s_colorMap["blue"] = COLOR_BLUE;
    s_colorMap["magenta"] = COLOR_MAGENTA;
    s_colorMap["cyan"] = COLOR_CYAN;
    s_colorMap["white"] = COLOR_WHITE;
    
    SetupEnhancedPalette();
  }
}

int TerminalColors::CreateColorPair(int fg, int bg)
{
  if (!has_colors() || s_nextColorPair >= COLOR_PAIRS) {
    return 0;
  }
  
  init_pair(s_nextColorPair, fg, bg);
  return COLOR_PAIR(s_nextColorPair++);
}

int TerminalColors::GetColorCode(const std::string& colorName)
{
  auto it = s_colorMap.find(colorName);
  return (it != s_colorMap.end()) ? it->second : COLOR_WHITE;
}

void TerminalColors::SetupEnhancedPalette()
{
  if (can_change_color() && COLORS >= 256) {
    // Define custom colors for better email display
    init_color(16, 45, 55, 72);   // Dark blue-gray background
    init_color(17, 96, 165, 250); // Light blue for links
    init_color(18, 104, 211, 145); // Green for success
    init_color(19, 245, 101, 101); // Red for errors/urgent
    init_color(20, 251, 191, 36);  // Yellow for warnings
    init_color(21, 160, 174, 192); // Light gray for metadata
    
    s_colorMap["email_bg"] = 16;
    s_colorMap["link"] = 17;
    s_colorMap["success"] = 18;
    s_colorMap["error"] = 19;
    s_colorMap["warning"] = 20;
    s_colorMap["metadata"] = 21;
  }
}
