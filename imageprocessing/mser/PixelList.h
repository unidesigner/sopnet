#ifndef IMAGEPROCESSING_MSER_PIXEL_LIST_H__
#define IMAGEPROCESSING_MSER_PIXEL_LIST_H__

struct PixelList {

	static const int None = -1;

	PixelList() {};

	PixelList(size_t size) :
		prev(size, None),
		next(size, None) {}

	void resize(size_t size) {

		prev.resize(size);
		next.resize(size);
	}

	size_t size() const {

		return prev.size();
	}

	std::vector<size_t> prev;
	std::vector<size_t> next;
};

#endif // IMAGEPROCESSING_MSER_PIXEL_LIST_H__

