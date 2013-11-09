A=baz
B=d

foo: bar baz
	echo $(PATH)
	@echo foo
#comment
bar: baz
$A: $B
