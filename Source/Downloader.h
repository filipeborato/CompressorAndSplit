#include "../JuceLibraryCode/JuceHeader.h"

struct Downloader : juce::URL::DownloadTask::Listener
{
	explicit Downloader(const juce::URL& downloadURL)
		: url(downloadURL)
	{
	}

	void downloadToFile(const juce::File& destinationFile)
	{
		url.downloadToFile(destinationFile, {}, this);
	}

	void finished(juce::URL::DownloadTask*, bool success) override
	{
		if (onFinish)
			onFinish();
	}

	std::function<void()> onFinish;

private:
	juce::URL url;
	std::unique_ptr<juce::URL::DownloadTask> task;
};
