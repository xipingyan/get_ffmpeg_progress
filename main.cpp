#include <string>
#include <mutex>
#include <stdio.h>

#ifdef WIN32
#include <Windows.h>
#else
#endif

class CMyProgress {
public:
	void set_dur(int val) {
		std::lock_guard<std::mutex> lk(_mutex);
		_duration = val;
	}
	int get_dur() {
		std::lock_guard<std::mutex> lk(_mutex);
		return _duration;
	}
	void set_tm(int val) {
		std::lock_guard<std::mutex> lk(_mutex);
		_tm = val;
	}
    void set_finish() {
		std::lock_guard<std::mutex> lk(_mutex);
		_bfinish = true;
	}
	bool get_finish() {
		std::lock_guard<std::mutex> lk(_mutex);
		return _bfinish;
	}

	float get_progress() {
		std::lock_guard<std::mutex> lk(_mutex);
		return _duration > 0 ? static_cast<float>(_tm) / static_cast<float>(_duration) : -1.f;
	}
private:
	std::mutex _mutex;
	int _tm = 0;
    int _duration = 0;
    bool _bfinish = false;
};

static CMyProgress g_myprogess;

// Format = hh:mm::ss.
bool parse_time(char* pBuffer, int& h, int& m, int& s) {
    // Hour
    auto hour = std::string(pBuffer).find_first_of(":", 0);
    if (hour != std::string::npos) {
        std::string sh, sm, ss;
        sh = std::string(pBuffer).substr(1, hour - 1);
        // Minute
        auto minute = std::string(pBuffer).find_first_of(":", hour + 1);
        if (minute != std::string::npos) {
            sm = std::string(pBuffer).substr(hour + 1, minute - hour - 1);
            // Second.
            auto second = std::string(pBuffer).find_first_of(".", minute + 1);
            if (second != std::string::npos) {
                ss = std::string(pBuffer).substr(minute + 1, second - minute - 1);
                //printf("sh=%s\n", sh.c_str());
                //printf("sm=%s\n", sm.c_str());
                //printf("ss=%s\n", ss.c_str());
				h = std::stoi(sh);
				m = std::stoi(sm);
                s = std::stoi(ss);
                return true;
            }
        }
    }
    return false;
}

// Format="xxxxxtime=hh:mm:ss.*****"
int get_cur_tm(char* pBuffer) {
	auto found = std::string(pBuffer).find("time=");
	if (found != std::string::npos)
	{
		found += 4;
        int h = 0, m = 0, s = 0;
		if(parse_time(pBuffer + found, h, m, s)) {
            return h * 60 * 60 + m * 60 + s;
        }
	}
	return 0;
}

void unit_test_for_parse_time() {
	char ptext[] = "=   29696kB time=00:00:30.88 bitrate=7875.6kbits/s speed=0.687x";
	auto x = get_cur_tm(ptext);
	if (x != 30) {
		printf("get_cur_tm parse fail, [%d!=30]\n", x);
	}
}

// Format="xxxxxDuration: hh:mm:ss.*****"
int get_duration(char* pBuffer) {
	auto found = std::string(pBuffer).find("Duration:");
	if (found != std::string::npos)
	{
		found += 9;
        int h = 0, m = 0, s = 0;
		if(parse_time(pBuffer + found, h, m, s)) {
            return h * 60 * 60 + m * 60 + s;
        }
	}
	return 0;
}

#ifdef _WIN32
int CreateProcess_WIN()
{
	PROCESS_INFORMATION pi;
	STARTUPINFO si;
	BOOL ret = false;
	DWORD flags = CREATE_NO_WINDOW;

	char pBuffer[256];
	SECURITY_ATTRIBUTES sa;
	sa.nLength = sizeof(SECURITY_ATTRIBUTES);
	sa.lpSecurityDescriptor = NULL;
	sa.bInheritHandle = true;
	HANDLE hReadPipe, hWritePipe;
	if(!CreatePipe(&hReadPipe, &hWritePipe, &sa, 0)) {
        printf("CreatePipe failed.\n");
        return 0;
    }

	ZeroMemory(&pi, sizeof(PROCESS_INFORMATION));
	ZeroMemory(&si, sizeof(STARTUPINFO));
	si.cb = sizeof(STARTUPINFO);
	si.dwFlags |= STARTF_USESTDHANDLES;
	si.hStdInput = NULL;
	si.hStdError = hWritePipe;
	si.hStdOutput = hWritePipe;
	
    // -c:v h264_qsv
	TCHAR cmd[] = TEXT("ffmpeg.exe -y -i test.mp4 file.avi");
	ret = CreateProcessA(NULL, cmd, NULL, NULL, TRUE, flags, NULL, NULL, &si, &pi);
	if (ret)
	{
		while (true)
		{
			DWORD ExitCode = 0;
			// Judge if process "ffmpeg" finish or not.
			GetExitCodeProcess(pi.hProcess, &ExitCode);
			if (ExitCode == STILL_ACTIVE)
			{
				DWORD rSize = 0;
				BOOL bRun = 0;
				bRun = ReadFile(hReadPipe, pBuffer, 255, &rSize, NULL);
				if (!bRun) {
					printf("ReadFile return false.\n");
					continue;
				}
				pBuffer[rSize] = '\0';

				static int dur = -1;
                if (g_myprogess.get_dur() == 0) {
					dur = get_duration(pBuffer);
					if (dur > 0) {
						g_myprogess.set_dur(dur);
					}
                }
				if (rSize > 20) {
					auto tm = get_cur_tm(pBuffer);
					if (tm > 0) {
						g_myprogess.set_tm(tm);
						if (tm >= dur) {
							printf("FFMpeg progress show finish.\n");
							//g_myprogess.set_finish();
							break;
						}
					}
				}
			}
			else
			{
				printf("FFMpeg Done.\n");
                g_myprogess.set_finish();
				break;
			}
		}

		printf("WaitForSingleObject...\n");
		WaitForSingleObject(pi.hProcess, INFINITE);
		printf("Finish CreateProcess_WIN\n");
		CloseHandle(pi.hProcess);
		CloseHandle(pi.hThread);
		return 0;
	}
	printf("Failure: please check if exist 'ffmpeg.exe'\n");
	return -1;
}

int main()
{
	std::thread thrd = std::thread([]() { CreateProcess_WIN(); });
	thrd.detach();
	for (;;) {
		static float g_p = 0;
		float cur_p = g_myprogess.get_progress();
		if ((cur_p != g_p) && cur_p >= 0) {
			g_p = cur_p;
			printf("Convert progress: %.2f%%\n", cur_p * 100.f);
		}

        if(g_myprogess.get_finish()) {
            break;
        }
		std::this_thread::sleep_for(std::chrono::milliseconds(100));
	}
    printf("Finish main.\n");
	return 0;
}
#else
#message("Not implemented.")
#endif