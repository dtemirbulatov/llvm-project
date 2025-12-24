// RUN: callreport -call-report-sourcepath="dump-calls" -call-report-output=%t.json %s | FileCheck %s --input-file=%t.json

extern void bar();
extern void circle();

int foo() {
  bar();
  circle();
// CHECK: "bar"
// CHECK: "circle"
  return 0;
}
