; RUN: opt < %s -instcombine -S | grep select

define double @fold(i1 %a, double %b) {
%s = select i1 %a, double 0., double 1.
%c = fdiv double %b, %s
ret double %c
}
