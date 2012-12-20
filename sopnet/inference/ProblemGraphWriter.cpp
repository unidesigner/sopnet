#include <fstream>

#include <boost/lexical_cast.hpp>

#include <vigra/impex.hxx>

#include "ProblemGraphWriter.h"

static logger::LogChannel problemgraphwriterlog("problemgraphwriterlog", "[ProblemGraphWriter] ");

util::ProgramOption optionProblemGraphFile(
		util::_module           = "sopnet",
		util::_long_name        = "problemGraphFile",
		util::_description_text = "Path to the problem graph file to produce.",
		util::_default_value    = "problem.graph");

ProblemGraphWriter::ProblemGraphWriter() {

	registerInput(_segments, "segments");
	registerInput(_segmentIdsToVariables, "segment ids map");
	registerInput(_objective, "objective");
	registerInputs(_linearConstraints, "linear constraints");
}

void
ProblemGraphWriter::write(
		const std::string& slicesFile,
		const std::string& segmentsFile,
		const std::string& constraintsFile,
		const std::string& sliceImageDirectory) {

	if (!_segments || !_segmentIdsToVariables || !_objective) {

		LOG_DEBUG(problemgraphwriterlog) << "not all required inputs are present -- skip dumping" << std::endl;
		return;
	}

	updateInputs();

	LOG_DEBUG(problemgraphwriterlog) << "dumping problem graph..." << std::endl;

	writeSlices(slicesFile, sliceImageDirectory);

	writeSegments(segmentsFile);

	writeConstraints();

	LOG_DEBUG(problemgraphwriterlog) << "done" << std::endl;
}

void
ProblemGraphWriter::writeSlices(
		const std::string& slicesFile,
		const std::string& sliceImageDirectory) {

	LOG_DEBUG(problemgraphwriterlog) << "writing slices to " << slicesFile << std::endl;

	std::ofstream out(slicesFile.c_str());

	out << "# id section bb.minX bb.maxX bb.minY bb.maxY value center.x center.y size" << std::endl;

	foreach (boost::shared_ptr<EndSegment> end, _segments->getEnds()) {

		if (end->getDirection() == Left)
			writeSlice(*end->getSlice(), out);
	}

	LOG_DEBUG(problemgraphwriterlog) << "writing slice images to " << sliceImageDirectory << std::endl;

	foreach (boost::shared_ptr<EndSegment> end, _segments->getEnds()) {

		if (end->getDirection() == Left)
			writeSliceImage(*end->getSlice(), sliceImageDirectory);
	}
}

void
ProblemGraphWriter::writeSegments(const std::string& segmentsFile) {

	LOG_DEBUG(problemgraphwriterlog) << "writing segments to " << segmentsFile << std::endl;

	std::ofstream out(segmentsFile.c_str());

	out << "# segmentid number_of_slices (1=end,2=continuation,3=branch) (sliceids)* cost" << std::endl;

	foreach (boost::shared_ptr<EndSegment> end, _segments->getEnds())
		writeSegment(*end, out);

	foreach (boost::shared_ptr<ContinuationSegment> continuation, _segments->getContinuations())
		writeSegment(*continuation, out);

	foreach (boost::shared_ptr<BranchSegment> branch, _segments->getBranches())
		writeSegment(*branch, out);

	out.close();
}

void
ProblemGraphWriter::writeConstraints() {

	LOG_DEBUG(problemgraphwriterlog) << "writing constraints" << std::endl;
}

void
ProblemGraphWriter::writeSlice(const Slice& slice, std::ofstream& out) {

	LOG_DEBUG(problemgraphwriterlog) << "writing slices" << std::endl;

	out << slice.getId() << " ";
	out << slice.getSection() << " ";
	out << slice.getComponent()->getBoundingBox().minX << " ";
	out << slice.getComponent()->getBoundingBox().maxX << " ";
	out << slice.getComponent()->getBoundingBox().minY << " ";
	out << slice.getComponent()->getBoundingBox().maxY << " ";
	out << slice.getComponent()->getValue() << " ";
	out << slice.getComponent()->getCenter().x << " ";
	out << slice.getComponent()->getCenter().y << " ";
	out << slice.getComponent()->getSize() << " ";
	out << std::endl;
}

void
ProblemGraphWriter::writeSliceImage(const Slice& slice, const std::string& sliceImageDirectory) {

	unsigned int section = slice.getSection();
	unsigned int id      = slice.getId();

	std::string filename = sliceImageDirectory + "/" + boost::lexical_cast<std::string>(section) + "_" + boost::lexical_cast<std::string>(id) + ".png";

	vigra::exportImage(vigra::srcImageRange(slice.getComponent()->getBitmap()), vigra::ImageExportInfo(filename.c_str()));
}

void
ProblemGraphWriter::writeSegment(const Segment& segment, std::ofstream& out) {

	LOG_ALL(problemgraphwriterlog) << "writing segment " << segment.getId() << std::endl;

	out << segment.getId() << " ";

	out << segment.getSlices().size();

	foreach (boost::shared_ptr<Slice> slice, segment.getSlices()) {

		out << " " << slice->getId();
	}

	out << " " << _objective->getCoefficients()[(*_segmentIdsToVariables)[segment.getId()]];

	out << std::endl;
}
