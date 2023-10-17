#include <string>
#include <mutex>
#include <stdio.h>

#ifdef WIN32
#include <Windows.h>
#else
#endif
#include "transcoding.hpp"

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

	bool have_error() {
		std::lock_guard<std::mutex> lk(_mutex);
		return _bHaveError;
	}
	void set_error() {
		std::lock_guard<std::mutex> lk(_mutex);
		_bHaveError = true;
	}
private:
	std::mutex _mutex;
	int _tm = 0;
    int _duration = 0;
    bool _bfinish = false;
	bool _bHaveError = false;
};

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

bool get_error(char* pBuffer) {
	auto found = std::string(pBuffer).find("Error");
	return found != std::string::npos;
}

#ifdef _WIN32
int CreateProcess_WIN(const std::string& src_fn, const std::string& dst_fn, 
	HANDLE hReadPipe, HANDLE hWritePipe, CMyProgress *myprogess)
{
	PROCESS_INFORMATION pi;
	STARTUPINFO si;
	BOOL ret = false;
	DWORD flags = CREATE_NO_WINDOW;

	char pBuffer[256];
	ZeroMemory(&pi, sizeof(PROCESS_INFORMATION));
	ZeroMemory(&si, sizeof(STARTUPINFO));
	si.cb = sizeof(STARTUPINFO);
	si.dwFlags |= STARTF_USESTDHANDLES;
	si.hStdInput = NULL;
	si.hStdError = hWritePipe;
	si.hStdOutput = hWritePipe;
	
    // -c:v h264_qsv
	std::string str_cmd = std::string("ffmpeg.exe -y -i \"") + src_fn + "\" \"" + dst_fn + "\"";
	TCHAR cmd[256] = { 0 };
	memcpy(cmd, str_cmd.c_str(), str_cmd.length());
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
				printf("----->pBuffer=%s\n", pBuffer);

				static int dur = -1;
                if (myprogess->get_dur() == 0) {
					dur = get_duration(pBuffer);
					if (dur > 0) {
						myprogess->set_dur(dur);
					}
                }
				if (rSize > 20) {
					auto tm = get_cur_tm(pBuffer);
					if (tm > 0) {
						myprogess->set_tm(tm);
						if (tm >= dur) {
							printf("FFMpeg progress show finish.\n");
							break;
						}
					}
				}

				if (get_error(pBuffer)) {
					myprogess->set_error();
					myprogess->set_finish();
				}
			}
			else
			{
				printf("FFMpeg Done.\n");
                myprogess->set_finish();
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


#else
#message("Not implemented.")
#endif


class CTranscodingImpl : public CTranscoding {
private:
	CMyProgress _myprogess;
	HANDLE hReadPipe = NULL, hWritePipe = NULL;

public:
	bool transcode(const std::string& src_fn, const std::string& dst_fn) {
		SECURITY_ATTRIBUTES sa;
		sa.nLength = sizeof(SECURITY_ATTRIBUTES);
		sa.lpSecurityDescriptor = NULL;
		sa.bInheritHandle = true;
		if (!CreatePipe(&hReadPipe, &hWritePipe, &sa, 0)) {
			printf("CreatePipe failed.\n");
			return false;
		}

		std::thread thrd = std::thread([&, src_fn, dst_fn]() { CreateProcess_WIN(src_fn, dst_fn, hReadPipe, hWritePipe, &_myprogess); });
		thrd.detach();
		return true;
	}
	float get_progress() {
		return _myprogess.get_progress();
	}

	bool is_finish() {
		// 
		DWORD rSize = 0;
		char ptext[] = "time=-1:-1:-1.88 bitrate=";
		if(!WriteFile(hWritePipe, ptext, strlen(ptext), &rSize, NULL)) {
			PRINT_ERR("Write fail.");
			return true;
		}

		return _myprogess.get_finish();
	}

	bool have_error() {
		return _myprogess.have_error();
	}
};

CTranscoding::Ptr create_transcoding() {
	return std::make_shared<CTranscodingImpl>();
}