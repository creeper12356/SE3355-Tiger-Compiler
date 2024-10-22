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