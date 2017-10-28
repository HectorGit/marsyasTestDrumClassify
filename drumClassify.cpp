#pragma once

#include "stdafx.h"

#include "drumClassify.h"
CommandLineOptions cmd_options;
string toy_withName;
int helpopt;
int usageopt;
int verboseopt;

//ref - toy_with_stereoMFCC line 3118 in mudbox.cpp
// and other examples in that file. (included @ bottom of this file)

DrumFeatureExtractor::DrumFeatureExtractor()
{
}

DrumFeatureExtractor::~DrumFeatureExtractor()
{
}


/*

I AM ELIMINATING THE NEED FOR A .MPL OUTPUT FILE
I JUST WANT THE FEATURES 

This code works by loading an mpl file with a trained classifier.

First run bextract to analyze examples and train a classifier
bextract -e DRUMEXTRACT -f outputfile.mpl -w 512 -sr 44100.0

Then run
drumextract outputfile.mpl
*/
void DrumFeatureExtractor::extractDrumFeatures(string sfName1)
{

	//STEPS
	//0. preprocess - create settings.
	int windowsize = 512;
	//int numberOfCoefficients = 67;
	
	//1. CREATE A NETWORK OF MARSYSTEMS EXTRACTING VARIOUS FEATURES
	MarSystemManager theManager;

	MarSystem* timeLoopNet = theManager.create("Series", "SER 1"); //called timeloop

	//element 1 - THE AUDIO FILE
	timeLoopNet->addMarSystem(theManager.create("SoundFileSource/src"));
	//timeLoopNet->addMarSystem(theManager.create("AudioSource", "src"));
	//timeLoopNet->linkControl("mrs_bool/hasData", "SoundFileSource/src/mrs_bool/hasData");

	//element 2 - THE PEAKER ? 
	timeLoopNet->addMarSystem(theManager.create("PeakerAdaptive", "peak"));

	//set the peaker parameters. 
	timeLoopNet->updControl("PeakerAdaptive/peak/mrs_natural/peakEnd", 512);
	timeLoopNet->updControl("PeakerAdaptive/peak/mrs_real/peakSpacing", 0.5);
	timeLoopNet->updControl("PeakerAdaptive/peak/mrs_real/peakStrength", 0.7);
	timeLoopNet->updControl("PeakerAdaptive/peak/mrs_natural/peakStart", 0);
	timeLoopNet->updControl("PeakerAdaptive/peak/mrs_natural/peakStrengthReset", 2);
	timeLoopNet->updControl("PeakerAdaptive/peak/mrs_real/peakDecay", 0.9);
	timeLoopNet->updControl("PeakerAdaptive/peak/mrs_real/peakGain", 0.5);

	//2. CREATE A CONTROL THAT USES A FILE NAMED FILE.WAV
	timeLoopNet->linkControl("mrs_string/filename", "SoundFileSource/src/mrs_string/filename");
	timeLoopNet->updControl("mrs_string/filename", sfName1);

	//3. EXTRACT THE FEATURES
	MarSystem* theExtractorNet = theManager.create("Series", "theExtractorNet");
	
	MarSystem* spectimeFanout = theManager.create("Fanout", "spectimeFanout");
	spectimeFanout->addMarSystem(theManager.create("ZeroCrossings", "zcrs"));
	spectimeFanout->addMarSystem(theManager.create("Rms", "rms"));

	MarSystem* theSpectralNet = theManager.create("Series", "theSpectralNet");
	theSpectralNet->addMarSystem(theManager.create("Windowing", "ham"));
	theSpectralNet->addMarSystem(theManager.create("Spectrum", "spk"));
	theSpectralNet->addMarSystem(theManager.create("PowerSpectrum", "pspk"));

	MarSystem* featureFanout = theManager.create("Fanout", "featureFanout");
	featureFanout->addMarSystem(theManager.create("Centroid", "centroid"));
	featureFanout->addMarSystem(theManager.create("Rolloff", "rolloff"));
	featureFanout->addMarSystem(theManager.create("MFCC", "mfcc"));
	featureFanout->addMarSystem(theManager.create("Kurtosis", "kurtosis"));
	featureFanout->addMarSystem(theManager.create("Skewness", "skewness"));
	featureFanout->addMarSystem(theManager.create("SFM", "sfm"));
	featureFanout->addMarSystem(theManager.create("SCF", "scf"));

	// INTERCONNECT

	theSpectralNet->addMarSystem(featureFanout);      // theExtractorNet contains(spectTimeFanout which contains ((theSpectralNet which contains featureFanout)))
	spectimeFanout->addMarSystem(theSpectralNet);	  // if we place the weka sink at the highest level, it'd probably work.
	theExtractorNet->addMarSystem(spectimeFanout);    // in the example, they created a whole 'series' just for the wekasink though.
	//hypothetically,
	//add the weka sink to theExtractorNet, since it's the one that 'contains' all.
	////MarSystem* accum = theManager.create("Accumulator", "acc"); // needed for the weka out ! from the last example of this file
	//OR MarSystem* acc = mng.create("Accumulator", "acc"); ?!
	////accum->addMarSystem(theExtractorNet);

	////MarSystem* total = theManager.create("Series", "total");
	////total->addMarSystem(accum);
	////total->updControl("Accumulator/acc/mrs_natural/nTimes", 1000);
	//total->addMarSystem(statistics2);

	//total->addMarSystem(theManager.create("Annotator", "ann")); //what is this for
	////total->addMarSystem(theManager.create("WekaSink", "wsink"));

	//total->updControl("WekaSink/wsink/mrs_natural/nLabels", 3); //don't really have labels
	////total->updControl("WekaSink/wsink/mrs_natural/downsample", 1); //divides number of sample
	//total->updControl("WekaSink/wsink/mrs_string/labelNames", "garage,grunge,jazz,"); //shouldnt name nonexisting labels
	////total->updControl("WekaSink/wsink/mrs_string/filename", "drumClassifyOutput.arff");
	////total->updControl("mrs_natural/inSamples", 1024); //hmm... problemas?
	//total->updControl("Annotator/ann/mrs_natural/label", 0); //hmm no idea what this does.
	//you can set the label for the annotator. it adds a label to your output data.

	//MAYBE WE SHOULD CREATE THE ACCUMULATOR AND WEKA SINK BEFORE, AND HOOK THEM UP THERE

	// FOR SOME REASON SETTING UP THIS - might be unnecessary

	theExtractorNet->updControl("mrs_natural/inSamples", 512);
	theExtractorNet->updControl("mrs_natural/onSamples", 512);
	theExtractorNet->updControl("mrs_real/israte", 44100.0);
	theExtractorNet->updControl("mrs_real/osrate", 44100.0);

	realvec in1;
	realvec out1;
	realvec out2;

	//getting 'observations' - I don't think necessary
	//it is used later on though...
	in1.create(timeLoopNet->getctrl("mrs_natural/inObservations")->to<mrs_natural>(),
		timeLoopNet->getctrl("mrs_natural/inSamples")->to<mrs_natural>());

	out1.create(timeLoopNet->getctrl("mrs_natural/onObservations")->to<mrs_natural>(),
		timeLoopNet->getctrl("mrs_natural/onSamples")->to<mrs_natural>());

	out2.create(theExtractorNet->getctrl("mrs_natural/onObservations")->to<mrs_natural>(),
		theExtractorNet->getctrl("mrs_natural/onSamples")->to<mrs_natural>());
	
	//4. PRINT THEM FOR NOW WITH COUT - WE NEED A FILE SINK. ARFF OR MPL OR SMTH.
	//IN THE FUTURE, STORE THEM , OR PUT THEM OUT
	//TO A FILE SOMEHOW. IGNORE OTHER CODE.

	//Text file output - from chroma example
	//theNewSystem = theManager.create("RealvecSink", "TextFileOutput");
	//timeLoopNet->addMarSystem(theNewSystem);
	//theControlString = "RealvecSink/TextFileOutput/mrs_string/fileName";
	//timeLoopNet->updControl(theControlString, inTextFileName);

	//accum->addMarSystem(cnet);
	//net->addMarSystem(accum);

	//6. PUT CIN.GET TO MAKE THINGS GOOD.

	// ------------------------------------------------------------------
	// BELOW THIS LINE NO IDEA HOW I COULD USE IN THE IMPLEMENTATION
	//LOOK AT BEXTRACT.CPP HOW IS -sv DONE? single vector
	// ------------------------------------------------------------------

	// PeakerAdaptive looks for hits and then if it finds one reports a non-zero value
	// When a non-zero value is found theExtractorNet is ticked
	while (timeLoopNet->getctrl("SoundFileSource/src/mrs_bool/hasData")->to<mrs_bool>())
	{
		timeLoopNet->process(in1, out1); // THIS IS WEIRD
		for (int i = 0; i < windowsize; ++i)
		{
			if (out1(i) > 0)
			{
				theExtractorNet->process(out1, out2);
				//total->tick();//i don't think this will work.
				cout << *theExtractorNet << endl;
				cout << out2 << endl;
			}
		}
	}

	cin.get();
}

/* GOOD EXAMPLE 1

void toy_with_chroma(string inSoundFileName, string inTextFileName)
{
  MarSystemManager theManager;

  // ------------------------ DEFINE MARSYSTEM ------------------------
  MarSystem *timeLoopNet, *theNewSystem;
  timeLoopNet = theManager.create("Series", "SER1");

  // Add new MarSystems
  MarSystem* theDummy;
  theDummy = new Spectrum2ACMChroma("Anything");
  theManager.registerPrototype("Spectrum2ACMChroma",theDummy);

  // 1. Read sound file
  theNewSystem = theManager.create("SoundFileSource","Source");
  timeLoopNet->addMarSystem(theNewSystem);

  // 2. Convert stereo to mono
  theNewSystem = theManager.create("Stereo2Mono","ToMono");
  timeLoopNet->addMarSystem(theNewSystem);

  // (!!) Compensate for x0.5 in Stereo2Mono in case of mono file
  theNewSystem = theManager.create("Gain","Gain1");
  timeLoopNet->addMarSystem(theNewSystem);

  // 3. Downsample to ~8kHz
  theNewSystem = theManager.create("DownSampler","Resample");
  timeLoopNet->addMarSystem(theNewSystem);

  // 4. Store "windowsize" samples in buffer
  theNewSystem = theManager.create("ShiftInput","Buffer");
  timeLoopNet->addMarSystem(theNewSystem);

  // 5. Perform windowing on buffer values
  // This function includes a 'x 2/Sum(w(n))' normalization
     with 2/Sum(w(n)) = 2 x 1/FS x FS/Sum(w(n)) with
     - 2 = energy in positive spectrum = energy in full spectrum
     - 1/FS = spectrum normalization (?)
     - FS/Sum(w(n)) = compensate for window shape 
  theNewSystem = theManager.create("Windowing","Windowing");
  timeLoopNet->addMarSystem(theNewSystem);

  // 6. Transform to frequency domain
  theNewSystem = theManager.create("Spectrum","CompSpectrum");
  timeLoopNet->addMarSystem(theNewSystem);

  // (!!) Compensate for normalization in fft
  theNewSystem = theManager.create("Gain","Gain2");
  timeLoopNet->addMarSystem(theNewSystem);

  // 7. Compute amplitude spectrum
  theNewSystem = theManager.create("PowerSpectrum","AmpSpectrum");
  timeLoopNet->addMarSystem(theNewSystem);

  // 8. Compute chroma profile
  theNewSystem = theManager.create("Spectrum2ACMChroma","Spectrum2Chroma");
  timeLoopNet->addMarSystem(theNewSystem);

  // 11. Text file output
  theNewSystem = theManager.create("RealvecSink","TextFileOutput");
  timeLoopNet->addMarSystem(theNewSystem);

  // ------------------------ SET PARAMETERS ------------------------
  mrs_real theHopSize = 0.02f;
  mrs_real theFrameSize = 0.08f;			// 0.150
  mrs_real theTargetSampleRate = 8000.f;
  mrs_natural theFFTSize = 8192;

  // First!! declare source, because (most) parameters rely on sample rate
  mrs_string theControlString = "SoundFileSource/Source/mrs_string/filename";
  timeLoopNet->updControl(theControlString,inSoundFileName);

  theControlString = "SoundFileSource/Source/mrs_real/osrate";
  MarControlPtr theControlPtr = timeLoopNet->getctrl(theControlString);
  mrs_real theInSampleRate = theControlPtr->to<mrs_real>();

  mrs_natural theHopNrOfSamples = (mrs_natural)floor(theHopSize*theInSampleRate+0.5);
  theControlString = "mrs_natural/inSamples";		// Why not "SoundFileSource/../inSamples" ?
  timeLoopNet->updControl(theControlString,theHopNrOfSamples);

  theControlString = "Gain/Gain1/mrs_real/gain";
  timeLoopNet->updControl(theControlString,2.);

  mrs_natural theFactor = (mrs_natural)floor(theInSampleRate/theTargetSampleRate);
  theControlString = "DownSampler/Resample/mrs_natural/factor";
  timeLoopNet->updControl(theControlString,theFactor);

  mrs_natural theFrameNrOfSamples = (mrs_natural)floor(theFrameSize*theInSampleRate+0.5);
  theControlString = "ShiftInput/Buffer/mrs_natural/winSize";
  timeLoopNet->updControl(theControlString,theFrameNrOfSamples);

  theControlString = "Windowing/Windowing/mrs_natural/zeroPadding";
  timeLoopNet->updControl(theControlString,theFFTSize-theFrameNrOfSamples);
  // Default: Hamming window

  theControlString = "Windowing/Windowing/mrs_bool/normalize";
  timeLoopNet->updControl(theControlString,true);

  theControlString = "Gain/Gain2/mrs_real/gain";
  timeLoopNet->updControl(theControlString,(mrs_real)theFFTSize);

  theControlString = "PowerSpectrum/AmpSpectrum/mrs_string/spectrumType";
  timeLoopNet->updControl(theControlString,"magnitude");

  theControlString = "RealvecSink/TextFileOutput/mrs_string/fileName";
  timeLoopNet->updControl(theControlString,inTextFileName);

  // ------------------------ COMPUTE CHROMA PROFILES ------------------------
  clock_t start = clock();
  theControlString = "SoundFileSource/Source/mrs_bool/hasData";
  while (timeLoopNet->getctrl(theControlString)->to<mrs_bool>())
    timeLoopNet->tick();

  clock_t finish = clock();
  cout << "Duration: " << (double)(finish-start)/CLOCKS_PER_SEC << endl;

  // For debugging
  //vector<string> theSupportedMarSystems = theManager.registeredPrototypes();
  //map<string,MarControlPtr> theMap = timeLoopNet->getControls();

  //timeLoopNet->tick();
  //timeLoopNet->tick();
  //timeLoopNet->tick();
  //timeLoopNet->tick();

  delete timeLoopNet;
}*/

/*
	GOOD EXAMPLE 2
	void
	toy_with_centroid(string sfName1)
	{
	cout << "Toy with centroid " << sfName1 << endl;
	MarSystemManager theManager;

	MarSystem* net = theManager.create("Series/net");
	MarSystem* accum = theManager.create("Accumulator/accum");

	MarSystem* cnet = theManager.create("Series/cnet");
	cnet->addMarSystem(theManager.create("SoundFileSource/src"));
	cnet->addMarSystem(theManager.create("Gain/gain"));
	cnet->addMarSystem(theManager.create("Windowing/ham"));
	cnet->addMarSystem(theManager.create("Spectrum/spk"));
	cnet->addMarSystem(theManager.create("PowerSpectrum/pspk"));
	cnet->addMarSystem(theManager.create("Centroid/cntrd"));
	cnet->linkControl("mrs_string/filename", "SoundFileSource/src/mrs_string/filename");

	accum->addMarSystem(cnet);
	net->addMarSystem(accum);

	accum->updControl("mrs_string/mode", "explicitFlush");
	cnet->updControl("mrs_string/filename", sfName1);

	//mrs_real val = 0.0;

	ofstream ofs;
	ofs.open("centroid.mpl");
	ofs << *cnet << endl;
	ofs.close();

	net->updControl("Accumulator/accum/mrs_natural/maxTimes", 2000);
	net->updControl("Accumulator/accum/mrs_natural/timesToKeep", 1);
	net->linkControl("Accumulator/accum/mrs_bool/flush",
	"Accumulator/accum/Series/cnet/SoundFileSource/src/mrs_bool/currentCollectionNewFile");
	net->updControl("Accumulator/accum/Series/cnet/SoundFileSource/src/mrs_real/duration", 0.5);

	while(net->getControl("Accumulator/accum/Series/cnet/SoundFileSource/src/mrs_bool/hasData")->to<mrs_bool>())
	{
		cout << net->getControl("Accumulator/accum/Series/cnet/SoundFileSource/src/mrs_string/currentlyPlaying")->to<mrs_string>() << endl;

		net->tick();
		const mrs_realvec& src_data =
		net->getctrl("mrs_realvec/processedData")->to<mrs_realvec>();
		cout << src_data << endl;
	}

}
*/


/*
void
toy_with_stereoMFCC(string fname0, string fname1)
{
MarSystemManager mng;

MarSystem* playbacknet = mng.create("Series", "playbacknet");
playbacknet->addMarSystem(mng.create("SoundFileSource", "src"));

MarSystem* stereobranches = mng.create("Parallel", "stereobranches");
MarSystem* left = mng.create("Series", "left");
MarSystem* right = mng.create("Series", "right");

left->addMarSystem(mng.create("Windowing", "hamleft"));
left->addMarSystem(mng.create("Spectrum", "spkleft"));
left->addMarSystem(mng.create("PowerSpectrum", "leftpspk"));
left->addMarSystem(mng.create("MFCC", "leftMFCC"));
left->addMarSystem(mng.create("TextureStats", "leftTextureStats"));

right->addMarSystem(mng.create("Windowing", "hamright"));
right->addMarSystem(mng.create("Spectrum", "spkright"));
right->addMarSystem(mng.create("PowerSpectrum", "rightpspk"));
right->addMarSystem(mng.create("MFCC", "rightMFCC"));
right->addMarSystem(mng.create("TextureStats", "rightTextureStats"));

stereobranches->addMarSystem(left);
stereobranches->addMarSystem(right);

playbacknet->addMarSystem(stereobranches);

MarSystem* acc = mng.create("Accumulator", "acc");
acc->addMarSystem(playbacknet);

MarSystem* statistics2 = mng.create("Fanout", "statistics2");
statistics2->addMarSystem(mng.create("Mean", "mn"));
statistics2->addMarSystem(mng.create("StandardDeviation", "std"));

MarSystem* total = mng.create("Series", "total");
total->addMarSystem(acc);
total->updControl("Accumulator/acc/mrs_natural/nTimes", 1000);
total->addMarSystem(statistics2);

total->addMarSystem(mng.create("Annotator", "ann"));
total->addMarSystem(mng.create("WekaSink", "wsink"));


total->updControl("WekaSink/wsink/mrs_natural/nLabels", 3);
total->updControl("WekaSink/wsink/mrs_natural/downsample", 1);
total->updControl("WekaSink/wsink/mrs_string/labelNames", "garage,grunge,jazz,");
total->updControl("WekaSink/wsink/mrs_string/filename", "stereoMFCC.arff");

playbacknet->updControl("SoundFileSource/src/mrs_string/filename", fname0);
playbacknet->linkControl("mrs_bool/hasData", "SoundFileSource/src/mrs_bool/hasData");


total->updControl("mrs_natural/inSamples", 1024);


Collection l;
l.read(fname0);
total->updControl("Annotator/ann/mrs_natural/label", 0);

for (mrs_natural i=0; i < l.size(); ++i)
{
total->updControl("Accumulator/acc/Series/playbacknet/SoundFileSource/src/mrs_string/filename", l.entry(i));
// if (i==0)
///total->updControl("Accumulator/acc/Series/playbacknet/AudioSink/dest/mrs_bool/initAudio", true);
//
cout << "Processing " << l.entry(i) << endl;
total->tick();
cout << "i = " << i << endl;

  }

  Collection m;
  m.read(fname1);

  total->updControl("Annotator/ann/mrs_natural/label", 1);


  for (mrs_natural i = 0; i < m.size(); ++i)
  {
	  total->updControl("Accumulator/acc/Series/playbacknet/SoundFileSource/src/mrs_string/filename", m.entry(i));
	  cout << "Processing " << m.entry(i) << endl;
	  total->tick();
	  cout << "i=" << i << endl;
  }

  Collection n;
  n.read("j.mf");

  total->updControl("Annotator/ann/mrs_natural/label", 2);


  for (mrs_natural i = 0; i < n.size(); ++i)
  {
	  total->updControl("Accumulator/acc/Series/playbacknet/SoundFileSource/src/mrs_string/filename", n.entry(i));
	  cout << "Processing " << n.entry(i) << endl;
	  total->tick();
	  cout << "i=" << i << endl;
  }
}
*/
