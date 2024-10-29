# Lab2文档
>522031910456
## 如何处理注释
```
/* comment */
"/*" { adjust(); begin(StartCondition_::COMMENT); }
<COMMENT> "/*" { adjust(); ++ comment_level_; } 
<COMMENT> "*/" { adjust(); if(comment_level_ == 1) { begin(StartCondition_::INITIAL); }  else { -- comment_level_; }}
<COMMENT> \n {adjust(); errormsg_->Newline();}
<COMMENT>. { adjust(); }
```
使用flexc++的StartCondition的状态：`INITIAL`, `COMMENT`表示是否处于注释的内部。为了支持嵌套注释，在Scanner类中使用`comment_level_`成员记录注释的层数。
当前状态为`INITIAL`状态时，识别到`/*`模式，则表示注释开头，进入`COMMENT`状态；当前状态为`COMMENT`时，如果识别到`/*`模式，表示进入了更深一层的嵌套，如果识别到`*/`模式，表示退出了一层注释的嵌套，如果退出到最外层，则回到`INITIAL`状态。
值得注意的是，需要对分行注释进行处理。

## 如何处理字符串
```
/* string literal */
/* NOTE: when `more()` is called, there's no need to call `adjust()` */
\" { more(); begin(StartCondition_::STR); }
<STR>\" { adjust(); begin(StartCondition_::INITIAL); setMatched(handleEscape(matched())); return Parser::STRING; }
<STR>\n { more(); errormsg_->Newline(); }
<STR>\\.|.  { more(); }
```
与注释类似，使用flexc++的StartCondition的状态：`INITIAL`, `STR`表示是否处于字符串的内部。在这里，对于多行的字符串进行特殊处理。
另外，词法分析器需要对含有转义字符的字符串进行转义处理，这是lab2中比较tricky的部分，我在`scanner.h`中定义`escape`函数，对原始字符串进行转义处理：
```cpp
inline std::string handleEscape(const std::string &source) {
  // std::cout << "source: " << source << std::endl;
  assert(source.size() >= 2);
  auto str_content_len = source.size() - 2;
  bool escaping = false;
  std::string result_str;

  for(auto i = 1;i <= str_content_len; ++i) {
    if(!escaping) {
      if(source[i] != '\\') {
        // 不进行转义
        result_str.push_back(source[i]);
      } else {
        // 开始转义
        escaping = true;
      }

      continue;
    }

    // 处理转义
    if(source[i] == 't') {
      result_str.push_back('\t');
    } else if(source[i] == 'n') {
      result_str.push_back('\n');
    } else if(source[i] == '"') {
      result_str.push_back('"');
    } else if(source[i] == '\\') {
      result_str.push_back('\\');
    } 
      else if(isdigit(source[i])) {
      assert(i + 1 <= str_content_len);
      assert(i + 2 <= str_content_len);
      assert(isdigit(source[i + 1]));
      assert(isdigit(source[i + 2]));
      unsigned char ascii_code = (source[i] - '0') * 100 + (source[i + 1] - '0') * 10 + (source[i + 2] - '0');
      result_str.push_back(ascii_code);
      i += 2;
    } else if(isspace(source[i])) {

      while(true) {
        ++i;
        if(source[i] == '\\') {
          break;
        } else {
          assert(isspace(source[i]));
        }
      }
    } else if(source[i] == '^') {
      assert(i + 1 <= str_content_len);
      result_str.push_back(source[i + 1] - '@');
      ++i;
    } else {
      std::cout << "source: " << source << std::endl;
      std::cout << "unimplemented: " << int(source[i]) << std::endl;
      assert(false);
    }

    escaping = false;
  }
  return result_str;
}
```
使用bool变量`escaping`表示是否处于转义状态，从头开始扫描识别出的字符串字面值，如果`escaping`为真，原样输出字符，如果`escaping`为假，按照下面的几种情况对字符进行转义处理。
### 处理单个转义
单个转义字符只`\`后只有一个字符为转义字符，包含`\n`,`\t`,`\\`等。
### 处理十进制转义
`\`字符后接着三位数字字符，表示字符值为该三位数表示的十进制对应的字符。在这种情况下，读取的指针要向后多读取2位，并确保这两位都是数字字符。
### 处理含有^的转义
`\`字符后接着`^`字符，然后接着单个字符。在这种情况下，读取的指针要向后多读1位。
### 处理空白字符的转义
`\`字符后是一系列空白字符，使用一个`while`循环将指针一直向后读取，直到再读取到一个`\`为止，并忽略这段字符。

## 错误处理
```
 /* illegal input */
. {adjust(); errormsg_->Error(errormsg_->tok_pos_, "illegal token");}
```
遇到无法识别的token时，报错`illegal token`并继续往下词法分析。

## 文件结束处理
```
<COMMENT> <<EOF>> {adjust(); errormsg_->Error(errormsg_->tok_pos_, "unexpected end of file");}
<STR> <<EOF>> {adjust(); errormsg_->Error(errormsg_->tok_pos_, "unexpected end of file");}
```
在注释或者字符串结束之前，如果文件结束（遇到EOF），报错`unexpected end of file`。

## 其他有趣的特性
无 ;)