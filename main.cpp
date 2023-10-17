#include <stdio.h>
#include <chrono>
#include <thread>

#include "transcoding.hpp"

int main()
{
	auto ptr = create_transcoding();
	if (!ptr->transcode("test2.mp4", "file2.avi")) {
		printf("transcode failed.");
		return 0;
	}

	for (;;) {
		printf("Convert progress: %.2f%%\n", ptr->get_progress() * 100.f);

		if (ptr->is_finish()) {
			printf("Main: %s\n", ptr->have_error() ? "Have error." : "is_finish.");
			break;
		}
		std::this_thread::sleep_for(std::chrono::milliseconds(100));
	}
	printf("Finish main.\n");
	return 0;
}