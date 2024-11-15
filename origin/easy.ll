; ModuleID = 'easy'
source_filename = "easy"
target triple = "x86_64-pc-linux-gnu"

define i32 @main() {
  %1 = alloca i32, align 4
  %2 = alloca i32, align 4
  %3 = alloca i32, align 4
  store i32 0, i32* %1, align 4
  store i32 1, i32* %2, align 4
  store i32 2, i32* %3, align 4
  %4 = load i32, i32* %2, align 4
  %5 = load i32, i32* %3, align 4
  %6 = icmp slt i32 %4, %5
  br i1 %6, label %7, label %8

7:                                                ; preds = %0
  store i32 3, i32* %3, align 4
  br label %8

8:                                                ; preds = %7, %0
  %9 = load i32, i32* %2, align 4
  %10 = load i32, i32* %3, align 4
  %11 = add nsw i32 %9, %10
  ret i32 %11
}
