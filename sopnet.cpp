/**
 * sopnet main file. Initializes all objects, views, and visualizers.
 */

#include <iostream>
#include <string>
#include <boost/thread.hpp>
#include <boost/shared_ptr.hpp>
#include <boost/lexical_cast.hpp>
#include <boost/make_shared.hpp>
#include <boost/progress.hpp>

#include <exceptions.h>
#include <gui/ContainerView.h>
#include <gui/HorizontalPlacing.h>
#include <gui/ImageView.h>
#include <gui/RotateView.h>
#include <gui/Window.h>
#include <gui/ZoomView.h>
#include <inference/io/RandomForestHdf5Writer.h>
#include <imageprocessing/ImageExtractor.h>
#include <imageprocessing/SubStackSelector.h>
#include <imageprocessing/gui/ImageStackView.h>
#include <imageprocessing/io/ImageStackHdf5Reader.h>
#include <imageprocessing/io/ImageStackDirectoryReader.h>
#include <sopnet/Sopnet.h>
#include <sopnet/gui/SegmentsView.h>
#include <util/hdf5.h>
#include <util/ProgramOptions.h>

using std::cout;
using std::endl;
using namespace gui;
using namespace logger;

util::ProgramOption optionProjectName(
		_long_name        = "project",
		_short_name       = "p",
		_description_text = "The HDF5 project file.");

util::ProgramOption optionTraining(
		_long_name        = "train",
		_short_name       = "t",
		_description_text = "Train the segment random forest classifier.");

util::ProgramOption optionFirstSection(
		_module           = "sopnet",
		_long_name        = "firstSection",
		_description_text = "The number of the first section to process.",
		_default_value    = 0);

util::ProgramOption optionLastSection(
		_module           = "sopnet",
		_long_name        = "lastSection",
		_description_text = "The number of the last section to process. If set to -1, all sections after <firstSection> will be used.",
		_default_value    = -1);

util::ProgramOption optionShowGroundTruth(
		_module           = "sopnet",
		_long_name        = "showGroundTruth",
		_description_text = "Show a 3D view of the ground-truth.");

util::ProgramOption optionShowGoldStandard(
		_module           = "sopnet",
		_long_name        = "showGoldStandard",
		_description_text = "Show a 3D view of the gold-standard.");

util::ProgramOption optionShowNegativeSamples(
		_module           = "sopnet",
		_long_name        = "showNegativeSamples",
		_description_text = "Show a 3D view of all negative training samples.");

util::ProgramOption optionShowResult(
		_module           = "sopnet",
		_long_name        = "showResult",
		_description_text = "Show a 3D view of the result.");


void handleException(boost::exception& e) {

	LOG_ERROR(out) << "[window thread] caught exception: ";

	if (boost::get_error_info<error_message>(e))
		LOG_ERROR(out) << *boost::get_error_info<error_message>(e);

	if (boost::get_error_info<stack_trace>(e))
		LOG_ERROR(out) << *boost::get_error_info<stack_trace>(e);

	LOG_ERROR(out) << std::endl;

	LOG_ERROR(out) << "[window thread] details: " << std::endl
	               << boost::diagnostic_information(e)
	               << std::endl;

	exit(-1);
}

void processEvents(boost::shared_ptr<gui::Window> window) {

	LOG_USER(out) << " started as " << window->getCaption() << " at " << window.get() << std::endl;

	while (!window->closed()) {

		try {

			usleep(100);
			window->processEvents();

		} catch (boost::exception& e) {

			handleException(e);
		}
	}

	LOG_USER(out) << "[window thread] releasing shared pointer to window" << std::endl;

	LOG_USER(out) << "[window thread] quitting" << std::endl;
}

int main(int optionc, char** optionv) {

	try {

		/********
		 * INIT *
		 ********/

		// init command line parser
		util::ProgramOptions::init(optionc, optionv);

		// init logger
		LogManager::init();

		LOG_USER(out) << "[main] starting..." << std::endl;

		/*********
		 * SETUP *
		 *********/

		// create a window
		boost::shared_ptr<gui::Window> window = boost::make_shared<gui::Window>("sopnet");
		window->processEvents();

		// create sopnet pipeline
		boost::shared_ptr<Sopnet> sopnet = boost::make_shared<Sopnet>("projects dir not yet implemented");

		// create basic views
		boost::shared_ptr<ImageStackView> rawSectionsView = boost::make_shared<ImageStackView>();
		boost::shared_ptr<ImageStackView> membranesView   = boost::make_shared<ImageStackView>();
		boost::shared_ptr<ImageStackView> groundTruthView = boost::make_shared<ImageStackView>();
		boost::shared_ptr<SegmentsView>   resultView      = boost::make_shared<SegmentsView>();

		// add views to window
		boost::shared_ptr<ContainerView<HorizontalPlacing> > container = boost::make_shared<ContainerView<HorizontalPlacing> >();
		boost::shared_ptr<gui::ZoomView> zoomView = boost::make_shared<gui::ZoomView>();
		container->addInput(rawSectionsView->getOutput());
		container->addInput(membranesView->getOutput());
		container->addInput(groundTruthView->getOutput());
		zoomView->setInput(container->getOutput());
		window->setInput(zoomView->getOutput());

		// set input of result view
		resultView->setInput(sopnet->getOutput("solution"));
		boost::shared_ptr<RotateView> r1 = boost::make_shared<RotateView>();
		r1->setInput(resultView->getOutput());
		container->addInput(r1->getOutput());

		boost::shared_ptr<pipeline::ProcessNode> rawSectionsReader;
		boost::shared_ptr<pipeline::ProcessNode> membranesReader;
		boost::shared_ptr<pipeline::ProcessNode> groundTruthReader;

		// create image stack readers
		if (!optionProjectName) {

			// if no project filename was given, try to read from default
			// directoryies
			rawSectionsReader = boost::make_shared<ImageStackDirectoryReader>("./raw/");
			membranesReader   = boost::make_shared<ImageStackDirectoryReader>("./membranes/");
			groundTruthReader = boost::make_shared<ImageStackDirectoryReader>("./groundtruth/");

		} else {

			// get the project filename
			std::string projectFilename = optionProjectName;

			// try to read from project hdf5 file
			rawSectionsReader = boost::make_shared<ImageStackHdf5Reader>(projectFilename, "vncstack", "raw");
			membranesReader   = boost::make_shared<ImageStackHdf5Reader>(projectFilename, "vncstack", "membranes");
			groundTruthReader = boost::make_shared<ImageStackHdf5Reader>(projectFilename, "vncstack", "groundtruth");
		}

		// select a substack, if options are set
		if (optionFirstSection || optionLastSection) {

			int firstSection = optionFirstSection;
			int lastSection  = optionLastSection;

			// create section selectors
			boost::shared_ptr<SubStackSelector> rawSelector         = boost::make_shared<SubStackSelector>(firstSection, lastSection);
			boost::shared_ptr<SubStackSelector> membranesSelector   = boost::make_shared<SubStackSelector>(firstSection, lastSection);
			boost::shared_ptr<SubStackSelector> groundTruthSelector = boost::make_shared<SubStackSelector>(firstSection, lastSection);

			// set their inputs to the outputs of the section readers
			rawSelector->setInput(rawSectionsReader->getOutput());
			membranesSelector->setInput(membranesReader->getOutput());
			groundTruthSelector->setInput(groundTruthReader->getOutput());

			// sneakily pretend the selectors are the readers
			rawSectionsReader = rawSelector;
			membranesReader   = membranesSelector;
			groundTruthReader = groundTruthSelector;
		}

		rawSectionsView->setInput(rawSectionsReader->getOutput());
		membranesView->setInput(membranesReader->getOutput());
		groundTruthView->setInput(groundTruthReader->getOutput());

		sopnet->setInput("raw sections", rawSectionsReader->getOutput());
		sopnet->setInput("membranes", membranesReader->getOutput());
		sopnet->setInput("ground truth", groundTruthReader->getOutput());

		if (optionShowGroundTruth) {

			boost::shared_ptr<SegmentsView> groundTruthView = boost::make_shared<SegmentsView>();
			boost::shared_ptr<RotateView>   gtRotateView    = boost::make_shared<RotateView>();

			groundTruthView->setInput(sopnet->getOutput("ground truth segments"));
			gtRotateView->setInput(groundTruthView->getOutput());

			container->addInput(gtRotateView->getOutput());
		}

		if (optionShowGoldStandard) {

			boost::shared_ptr<SegmentsView> goldstandardView = boost::make_shared<SegmentsView>();
			boost::shared_ptr<RotateView>   gsRotateView     = boost::make_shared<RotateView>();

			goldstandardView->setInput(sopnet->getOutput("gold standard"));
			gsRotateView->setInput(goldstandardView->getOutput());

			container->addInput(gsRotateView->getOutput());
		}

		if (optionShowNegativeSamples) {

			boost::shared_ptr<SegmentsView> negativeView = boost::make_shared<SegmentsView>();
			boost::shared_ptr<RotateView>   neRotateView = boost::make_shared<RotateView>();

			negativeView->setInput(sopnet->getOutput("negative samples"));
			neRotateView->setInput(negativeView->getOutput());

			container->addInput(neRotateView->getOutput());
		}

		if (optionTraining) {

			boost::shared_ptr<RandomForestHdf5Writer> rfWriter = boost::make_shared<RandomForestHdf5Writer>("./segment_rf.hdf");

			rfWriter->setInput("random forest", sopnet->getOutput("random forest"));
			rfWriter->write();

		} else {

			while (!window->closed()) {

				window->processEvents();
				usleep(1000);
			}
		}

		LOG_USER(out) << "[main] exiting..." << std::endl;

	} catch (Exception& e) {

		handleException(e);
	}
}
