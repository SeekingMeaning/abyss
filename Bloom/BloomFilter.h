/**
 * A Bloom filter
 * Copyright 2013 Shaun Jackman
 */
#ifndef BLOOMFILTER_H
#define BLOOMFILTER_H 1

#include "Bloom/Bloom.h"
#include "Common/Kmer.h"
#include "Common/IOUtil.h"
#include <algorithm>
#include <vector>
#include <iostream>
#include <boost/dynamic_bitset.hpp>

/** A Bloom filter. */
class BloomFilter
{
  public:

	enum LoadType {
		LOAD_OVERWRITE,
		LOAD_UNION,
		LOAD_INTERSECT
	};

	static const size_t IO_BUFFER_SIZE = 32 * 1024;

	/** Constructor. */
	BloomFilter() { }

	/** Constructor. */
	BloomFilter(size_t n) : m_array(n) { }

	/** Return the size of the bit array. */
	size_t size() const { return m_array.size(); }

	/** Return the population count, i.e. the number of set bits. */
	size_t popcount() const
	{
		return m_array.count();
	}

	/** Return the estimated false positive rate */
	double FPR() const
	{
		return (double)popcount() / size();
	}

	/** Return whether the specified bit is set. */
	bool operator[](size_t i) const
	{
		assert(i < m_array.size());
		return m_array[i];
	}

	/** Return whether the object is present in this set. */
	bool operator[](const Bloom::key_type& key) const
	{
		return m_array[Bloom::hash(key) % m_array.size()];
	}

	/** Add the object with the specified index to this set. */
	void insert(size_t index)
	{
		assert(index < m_array.size());
		m_array[index] = true;
	}

	/** Add the object to this set. */
	void insert(const Bloom::key_type& key)
	{
		m_array[Bloom::hash(key) % m_array.size()] = true;
	}

	friend std::istream& operator>>(std::istream& in, BloomFilter& o)
	{
		o.read(in, LOAD_OVERWRITE);
		return in;
	}

	/** Read the bloom filter from a stream */
	void read(std::istream& in, LoadType loadType = LOAD_OVERWRITE,
			unsigned shrinkFactor = 1)
	{
		// read bloom filter file format version

		unsigned bloomVersion;
		in >> bloomVersion >> expect("\n");
		assert(in);
		if (bloomVersion != Bloom::BLOOM_VERSION) {
			std::cerr << "error: bloom filter version (`"
				<< bloomVersion << "'), does not match version required "
				"by this program (`" << Bloom::BLOOM_VERSION << "').\n";
			exit(EXIT_FAILURE);
		}

		// read bloom filter k value

		unsigned k;
		in >> k >> expect("\n");
		assert(in);
		if (k != Kmer::length()) {
			std::cerr << "error: this program must be run with the same kmer "
				"size as the bloom filter being loaded (k="
				<< k << ").\n";
			exit(EXIT_FAILURE);
		}

		// read bloom filter dimensions

		size_t size, startBitPos, endBitPos;
		readBloomDimensions(in, size, startBitPos, endBitPos);

		// shrink factor allows building a smaller
		// bloom filter from a larger one

		if (size % shrinkFactor != 0) {
			std::cerr << "error: the number of bits in the original bloom "
				"filter must be evenly divisible by the shrink factor (`"
				<< shrinkFactor << "')\n";
			exit(EXIT_FAILURE);
		}

		size /= shrinkFactor;

		if((loadType == LOAD_UNION || loadType == LOAD_INTERSECT)
			&& size != this->size()) {
			std::cerr << "error: can't union/intersect two bloom filters "
				"with different sizes.\n";
			exit(EXIT_FAILURE);
		} else {
			m_array.resize(size);
		}

		// read bit vector

		if (loadType == LOAD_OVERWRITE)
			m_array.reset();

		size_t offset = startBitPos;
		size_t bits = endBitPos - startBitPos + 1;
		size_t bytes = (bits + 7) / 8;

		char buf[IO_BUFFER_SIZE];
		for (size_t i = 0, j = offset; i < bytes; ) {
			size_t readSize = std::min(IO_BUFFER_SIZE, bytes - i);
			in.read(buf, readSize);
			assert(in);
			for (k = 0; k < readSize; k++) {
				for (unsigned l = 0; l < 8 && j < offset + bits; l++, j++) {
					bool bit = buf[k] & (1 << (7 - l));
					size_t index = j % size;
					switch (loadType)
					{
					case LOAD_OVERWRITE:
					case LOAD_UNION:
						m_array[index] |= bit;
						break;
					case LOAD_INTERSECT:
						m_array[index] &= bit;
						break;
					}
				}
			}
			i += readSize;
		}

	}
	
	/** Operator for writing the bloom filter to a stream */
	friend std::ostream& operator<<(std::ostream& out, const BloomFilter& o)
	{
		Bloom::write(o, out);
		return out;
	}

  protected:

	void readBloomDimensions(std::istream& in,
		size_t& size, size_t& startBitPos,
		size_t& endBitPos)
	{
		in >> size
		   >> expect("\t") >> startBitPos
		   >> expect("\t") >> endBitPos
		   >> expect("\n");

		assert(in);
		assert(startBitPos < size);
		assert(endBitPos < size);
		assert(startBitPos <= endBitPos);
	}

	boost::dynamic_bitset<> m_array;
};

#endif
