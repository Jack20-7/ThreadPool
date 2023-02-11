#获取当前目录下所有的.cc文件
src = $(wildcard ./*.cc)
#讲获取到的.cc去掉.cc后的名称
target = $(patsubst %.cc,%,$(src))

All:$(target)

%:%.cc
	g++ $< -o $@ -std=c++14 -g -l pthread

clean:
	rm -rf $(target)

