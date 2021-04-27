#pragma once

/// <summary>
/// A collection of constant size that will overwrite the first element when full
/// </summary>
template<class Type>
class RingBuffer
{
public:
	RingBuffer(size_t size = 30) : size(size), start(0), end(0), isEmpty(true)
	{
		buffer = new Type[size];
	}
	~RingBuffer()
	{
		delete[] buffer;
	}


	/// <summary>
	/// Get an element relitive to the first element
	/// </summary>
	Type& operator[](size_t index)
	{
		// Get index from start, looping around to the start
		size_t i = (start + index) % size;
		return buffer[i];
	}
	/// <summary>
	/// Get an element relitive to the first element
	/// </summary>
	const Type& operator[](size_t index) const
	{
		// Get index from start, looping around to the start
		size_t i = (start + index) % size;
		return buffer[i];
	}
	
	/// <summary>
	/// Push an element onto the end of the buffer. If the buffer is full, the first element will be overwritten
	/// </summary>
	void push(Type element)
	{
		size_t index = end % size;

		// If the buffer is full, move start forward
		if (getSize() == size)
		{
			start += 1;
			start %= size;
		}

		// Assign the element and update end
		buffer[index] = element;
		end = index + 1;
		// An element has been added, so its not empty
		isEmpty = false;
	}

	/// <summary>
	/// Returns the number of elements being used
	/// </summary>
	size_t getSize() const
	{
		// Start and end are equal when empty or full
		if (start == end)
		{
			return isEmpty ? 0 : size;
		}

		// Find the difference in positions
		size_t diff = end - start;
		if (start > end)
		{
			diff += size;
		}

		return diff;
	}


private:
	Type* buffer;

	const size_t size;

	size_t start;	// Read pos
	size_t end;		// Write pos

	bool isEmpty;
};