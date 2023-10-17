#pragma once
#include <memory>
#include <thread>
#include <string>

#define PRINT_ERR(str) printf("ERR: [%s:%d] %s\n", __FILE__, __LINE__, str) 
#define PRINT_LOG(str) printf("LOG: [%s:%d] %s\n", __FILE__, __LINE__, str)

class CTranscoding {
public:
	using Ptr = std::shared_ptr<CTranscoding>;
	CTranscoding() = default;
	~CTranscoding() = default;
	virtual bool transcode(const std::string& src_fn, const std::string& dst_fn) = 0;
	virtual float get_progress() = 0;
	virtual bool is_finish() = 0;
	virtual bool have_error() = 0;
};

CTranscoding::Ptr create_transcoding();