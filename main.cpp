#include <cstdio>
#include <malloc.h>
#include <list>

#include "user-thread.hpp"

void childfun(void* arg) {
	printf("childfun!\n");
	yield_thread();

	printf("childfun 1 \n");
	yield_thread();
	printf("childfun 2 \n");
	yield_thread();
	printf("childfun 3 \n");
	yield_thread();
	printf("childfun end!\n");
}


int main(int argc, char **argv) {
	printf("aaa\n");
	// jmp_buf env;
	
	printf("call 3 childfun\n");
	std::list<Thread> threads;
	threads.push_back(start_thread(childfun, 0));
	threads.push_back(start_thread(childfun, 0));
	threads.push_back(start_thread(childfun, 0));

	while (!threads.empty()) {
		for (auto it = threads.begin(), end = threads.end(); it != end;) {
			auto& thread = *it;
			printf("c: %p\n", &thread);
			if (thread.running()) {
				printf("continue childfun\n");
				thread.continue_();
			} else {
				printf("childfun finished\n");
				it = threads.erase(it);
				continue;

			}
			++it;
		}
	}

	printf("end 3 childfun\n");
}



