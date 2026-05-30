#include <iostream>
#include <vector>
#include <algorithm>
#include <cstdlib>   // rand()、srand()
#include <ctime>     // time()
#include <chrono>    // 高精度计时
#include "ScheduledThreadPool.hpp"
#include<future>
//#include"TimerManager.hpp"
//#include"Timer.hpp"

using namespace std;
using namespace std::chrono;
void func(){
    static int num=0;
    cout<<"func num: "<<++num<<endl;
}
void funb(){
    static int num=0;
    cout<<"funb num: "<<++num<<endl;
}
void funa(){
    static int num=0;
    cout<<"funa num: "<<++num<<endl;
}
int main(){
    ScheduledThreadPool mypool;
    mypool.excute(20,0,funa);
    mypool.excute(2,8,funb);
    mypool.excute(2,2,func);
        while(1){
        static int num=0;
        std::this_thread::sleep_for(std::chrono::seconds(1));
        cout<<"while seconds:"<<++num<<endl;
    }
    return 0;
}

#if 0
void func(){
    static int num=0;
    cout<<"func num: "<<++num<<endl;
}

void funb(){
    static int num=0;
    cout<<"funb num: "<<++num<<endl;
}
void funa(){
    static int num=0;
    cout<<"funa num: "<<++num<<endl;
}
int main(){
    Timer t1,t2,t3;
    TimerManager manager;
    t1.init();
    t1.set_timer(funa,2,2);
    t2.init();
    t2.set_timer(funb,2,4);
    t3.init();
    t3.set_timer(func,2,6);
    manager.init(-1);
    manager.add_timer(t1);
    manager.add_timer(t2);
    manager.add_timer(t3);
    while(1){
        static int num=0;
        std::this_thread::sleep_for(std::chrono::seconds(1));
        cout<<"while seconds:"<<++num<<endl;
    }
    return 0;
}


int main(){
    Timer timer;
    timer.init();
    timer.set_timer(func,2,5);
    for(int i=0;i<10;++i){
        timer.handle_event();
    }
    return 0;
}




void func(char ch, int x) {
	cout << "func_" << ch << "x" << x << endl;
}

int main() {

	ScheduledThreadPool mypool(100,2);
	mypool.excute(5,  std::bind(func, 'a', 5));
	mypool.excute(10, std::bind(func, 'b', 10));
	mypool.excute(15, std::bind(func, 'c', 15));
	mypool.excute(25, std::bind(func, 'd', 25));
	mypool.excute(2,  std::bind(func, 'e', 2));
	return 0;
}
#endif