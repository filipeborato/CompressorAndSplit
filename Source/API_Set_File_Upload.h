#include "../JuceLibraryCode/JuceHeader.h"

class API_Set_File_Upload : public ThreadWithProgressWindow {
public:
	API_Set_File_Upload(File file_to_upload, String host_name)
		: ThreadWithProgressWindow("Uploading file " + file_to_upload.getFileName(), true, true, 1000, "Cancel")
		, file_to_upload_(file_to_upload)
		, host_name_(host_name)
	{
	}

	virtual void run() override {

		if (!file_to_upload_.existsAsFile()) {
			response_str = "Upload file does not exist.";
			return;
		}

		// open a URL connection to you Rest server.
		String url_str(host_name_ + "/upload");
		URL url(url_str);

		url = url.withFileToUpload("audio", file_to_upload_, "application/octet-stream");
		URL::OpenStreamProgressCallback * callback = &API_Set_File_Upload::ProgressCallback;
		InputStream * input = url.createInputStream(true, callback, this);

		//String result_post = input->readEntireStreamAsString();
		//DBG("result post: " + result_post);
		DBG("Done");
		response_str += String("one upload done");

	}

	static bool ProgressCallback(void* context, int bytesSent, int totalBytes) {
		double progress_value = bytesSent / ((double)totalBytes);
		DBG("progress: " + String(progress_value));
		if (static_cast<API_Set_File_Upload*>(context)->currentThreadShouldExit())
			return false;
		static_cast<API_Set_File_Upload*>(context)->setProgress(progress_value);
		return true;
	}
	//String getResponseString() { ScopedLock l(responseLock); return response; }



protected:

	File file_to_upload_;
	String host_name_;
	String response_str = "ok";
	String responseLock;
	String response;

};
