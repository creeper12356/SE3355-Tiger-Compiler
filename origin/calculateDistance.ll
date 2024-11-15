; ModuleID = 'calculateDistance'
source_filename = "calculateDistance"
target triple = "x86_64-pc-linux-gnu"

%struct.Edge_s = type { i32, i32, i64 }

@edge1 = dso_local global %struct.Edge_s { i32 0, i32 0, i64 5 }, align 8
@edge2 = dso_local global %struct.Edge_s { i32 0, i32 0, i64 10 }, align 8
@allDist = dso_local global [3 x [3 x i32]] zeroinitializer, align 16
@dist = dso_local global [3 x %struct.Edge_s*] [%struct.Edge_s* @edge1, %struct.Edge_s* @edge2, %struct.Edge_s* null], align 16
@minDistance = dso_local global i64 5, align 8
@.str = private unnamed_addr constant [5 x i8] c"%lld\00", align 1
@.str1 = private unnamed_addr constant [14 x i8] c"%lld %lld %d\0A\00", align 1

declare i32 @__isoc99_scanf(i8* noundef, ...)

declare i32 @printf(i8* noundef, ...)

define void @caculateDistance() {
  %1 = alloca i32, align 4
  %2 = alloca i64, align 8
  store i32 0, i32* %1, align 4
  br label %3

3:                                                ; preds = %22, %0
  %4 = load i32, i32* %1, align 4
  %5 = icmp slt i32 %4, 3
  br i1 %5, label %6, label %25

6:                                                ; preds = %3
  %7 = load i32, i32* %1, align 4
  %8 = sext i32 %7 to i64
  %9 = getelementptr inbounds [3 x %struct.Edge_s*], [3 x %struct.Edge_s*]* @dist, i64 0, i64 %8
  %10 = load %struct.Edge_s*, %struct.Edge_s** %9, align 8
  %11 = getelementptr inbounds %struct.Edge_s, %struct.Edge_s* %10, i32 0, i32 2
  %12 = load i64, i64* %11, align 4
  store i64 %12, i64* %2, align 4
  %13 = load i64, i64* %2, align 4
  %14 = load i64, i64* @minDistance, align 4
  %15 = icmp slt i64 %13, %14
  br i1 %15, label %16, label %18

16:                                               ; preds = %6
  %17 = load i64, i64* %2, align 4
  br label %20

18:                                               ; preds = %6
  %19 = load i64, i64* @minDistance, align 4
  br label %20

20:                                               ; preds = %18, %16
  %21 = phi i64 [ %17, %16 ], [ %19, %18 ]
  store i64 %21, i64* @minDistance, align 4
  br label %22

22:                                               ; preds = %20
  %23 = load i32, i32* %1, align 4
  %24 = add nsw i32 %23, 1
  store i32 %24, i32* %1, align 4
  br label %3

25:                                               ; preds = %3
  ret void
}

define i32 @main() {
  %1 = alloca i32, align 4
  %2 = alloca %struct.Edge_s, align 8
  store i32 0, i32* %1, align 4
  %3 = getelementptr inbounds %struct.Edge_s, %struct.Edge_s* %2, i32 0, i32 2
  %4 = call i32 (i8*, ...) @__isoc99_scanf(i8* getelementptr inbounds ([5 x i8], [5 x i8]* @.str, i64 0, i64 0), i64* %3)
  store %struct.Edge_s* %2, %struct.Edge_s** getelementptr inbounds ([3 x %struct.Edge_s*], [3 x %struct.Edge_s*]* @dist, i64 0, i64 2), align 8
  %5 = getelementptr inbounds %struct.Edge_s, %struct.Edge_s* %2, i32 0, i32 2
  %6 = load i64, i64* %5, align 4
  %7 = trunc i64 %6 to i32
  store i32 %7, i32* getelementptr inbounds ([3 x [3 x i32]], [3 x [3 x i32]]* @allDist, i64 0, i64 0, i64 0), align 4
  call void @caculateDistance()
  %8 = load i64, i64* @minDistance, align 4
  %9 = getelementptr inbounds %struct.Edge_s, %struct.Edge_s* %2, i32 0, i32 2
  %10 = load i64, i64* %9, align 4
  %11 = add nsw i64 %10, 5
  %12 = add nsw i64 %11, 10
  %13 = load i32, i32* getelementptr inbounds ([3 x [3 x i32]], [3 x [3 x i32]]* @allDist, i64 0, i64 0, i64 0), align 4
  %14 = call i32 (i8*, ...) @printf(i8* getelementptr inbounds ([14 x i8], [14 x i8]* @.str1, i64 0, i64 0), i64 %8, i64 %12, i32 %13)
  ret i32 0
}
