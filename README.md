# CompressorAndSplit - Plugin Boratio - VST3

This project is a VST created from the Juce framework, with a DSP algorithm for compressing the audio track and sending the audio tracks to a server that hosts a neural network for separating the sound sources.

### For Building Follow the List:

1. Download the latest version of the Juce framework
2. Open Projucer and go to openFile and choose the CompressorAndSplit.jucer file
3. Open project with build button in Visual Studio, Xcode or Linux Makefile
4. Build

### or

1. Download the latest version of the Juce framework
2. Open Projucer and create a plugin project for Vst
3. Open project with build button in Visual Studio, Xcode or Linux Makefile
4. Copy the files from the souce folder to the source of your new project
5. Build

### This project is a plugin created with Juce Framework
    
This project has a python server, in the Flask framework, with a hosted neural network. The name of this Back End is Split and it's on my Github.

### The Server
To use the Splitter is necessary to run this project:
https://github.com/filipeborato/Split
    

