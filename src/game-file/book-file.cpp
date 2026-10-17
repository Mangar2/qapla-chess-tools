/**
 * @license
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.

 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.

 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 *
 * @author Volker Böhm
 * @copyright Copyright (c) 2026 Volker Böhm
 */

#include "book-file.h"

#include <charconv>
#include <cstdio>
#include <stdexcept>

namespace GameFile {

	namespace {

		enum HeaderTag : uint32_t {
			END_OF_HEADER = 0, AUTHOR = 1, NAME = 2, BOOK_VERSION = 3, FILE_VERSION = 4,
			PAYLOAD_ID = 5, NODE_SIZE = 6
		};

		/** The longest string a header record may hold, a broken file must not allocate. */
		constexpr uint32_t MAX_RECORD_LENGTH = 1 << 16;

		constexpr uint32_t HAS_CHILD = 0x0001;
		constexpr uint32_t HAS_RIGHT_SIBLING = 0x0002;
		constexpr uint32_t PRIORITY_SHIFT = 2;
		constexpr uint32_t PRIORITY_MASK = 0x1F;
		constexpr uint32_t MOVE_SHIFT = 7;
		constexpr uint32_t MOVE_MASK = 0x1FF;

		/** A file that closes itself, and knows its name for the messages. */
		class File {
		public:
			File(const std::string& path, const char* mode) : path_(path), file_(std::fopen(path.c_str(), mode)) {
				if (file_ == nullptr) {
					throw std::runtime_error(std::string(mode[0] == 'r' ? "cannot open " : "cannot create ") + path);
				}
			}
			~File() { if (file_ != nullptr) std::fclose(file_); }
			File(const File&) = delete;
			File& operator=(const File&) = delete;

			void read(void* data, size_t size, const char* where) {
				if (std::fread(data, 1, size, file_) != size) {
					throw std::runtime_error(path_ + " is not a book: it ends inside " + where);
				}
			}

			uint32_t readInt(const char* where) {
				uint8_t bytes[4];
				read(bytes, sizeof(bytes), where);
				return uint32_t(bytes[0]) | (uint32_t(bytes[1]) << 8) | (uint32_t(bytes[2]) << 16)
					| (uint32_t(bytes[3]) << 24);
			}

			void write(const void* data, size_t size) {
				if (std::fwrite(data, 1, size, file_) != size) failed_ = true;
			}

			void writeInt(uint32_t value) {
				const uint8_t bytes[4] = { uint8_t(value), uint8_t(value >> 8), uint8_t(value >> 16),
					uint8_t(value >> 24) };
				write(bytes, sizeof(bytes));
			}

			void writeRecord(HeaderTag tag, const std::string& value) {
				writeInt(tag);
				writeInt(uint32_t(value.size() + 1));
				write(value.c_str(), value.size() + 1);
			}

			void close() {
				if (std::fclose(file_) != 0) failed_ = true;
				file_ = nullptr;
				if (failed_) throw std::runtime_error("writing " + path_ + " failed");
			}

			const std::string& path() const { return path_; }

		private:
			std::string path_;
			std::FILE* file_;
			bool failed_ = false;
		};
	}

	Book::Book() {
		nodes_.push_back(Node{});
	}

	Book::NodeIndex Book::addMove(NodeIndex parent, uint32_t move) {
		for (NodeIndex child = nodes_[parent].firstChild; child != NO_NODE; child = nodes_[child].nextSibling) {
			if (nodes_[child].move == move) return child;
		}
		return appendChild(parent, Node{ .move = uint16_t(move) });
	}

	Book::NodeIndex Book::findMove(NodeIndex parent, uint32_t move) const {
		for (NodeIndex child = nodes_[parent].firstChild; child != NO_NODE; child = nodes_[child].nextSibling) {
			if (nodes_[child].move == move) return child;
		}
		return NO_NODE;
	}

	bool Book::sameTree(const Book& other) const {
		if (nodes_.size() != other.nodes_.size()) return false;
		for (size_t index = 1; index < nodes_.size(); ++index) {
			const Node& mine = nodes_[index];
			const Node& theirs = other.nodes_[index];
			if (mine.move != theirs.move || mine.priority != theirs.priority
				|| (mine.firstChild == NO_NODE) != (theirs.firstChild == NO_NODE)
				|| (mine.nextSibling == NO_NODE) != (theirs.nextSibling == NO_NODE)) {
				return false;
			}
		}
		return true;
	}

	Book::NodeIndex Book::appendChild(NodeIndex parent, const Node& node) {
		const NodeIndex child = NodeIndex(nodes_.size());
		nodes_.push_back(Node{ .move = node.move, .value = node.value, .score = node.score,
			.priority = node.priority });
		// The new node is a leaf, and a parent that had no child until now stops being one.
		if (nodes_[parent].firstChild == NO_NODE) {
			nodes_[parent].firstChild = child;
			if (parent != ROOT) --leaves_;
		}
		else {
			nodes_[nodes_[parent].lastChild].nextSibling = child;
		}
		nodes_[parent].lastChild = child;
		++leaves_;
		return child;
	}

	void Book::write(const std::string& path) const {
		if (header.payload > MAX_PAYLOAD) {
			throw std::runtime_error("a payload of " + std::to_string(header.payload) + " bytes is too large");
		}
		File file(path, "wb");
		file.writeRecord(AUTHOR, header.author);
		file.writeRecord(NAME, header.name);
		file.writeRecord(BOOK_VERSION, header.version);
		file.writeRecord(FILE_VERSION, BOOK9_FILE_VERSION);
		file.writeRecord(PAYLOAD_ID, header.payloadId);
		file.writeRecord(NODE_SIZE, std::to_string(2 + header.payload));
		file.writeInt(END_OF_HEADER);
		file.writeInt(uint32_t(size()));

		// Preorder: a stack of the nodes still to write, the next sibling below the first child.
		std::vector<uint8_t> data;
		std::vector<NodeIndex> stack;
		const size_t nodeSize = 2 + size_t(header.payload);
		const auto push = [&](NodeIndex index) { if (index != NO_NODE) stack.push_back(index); };
		push(nodes_[ROOT].firstChild);
		const bool scoreCounts = header.payloadId == SCORE_COUNT_PAYLOAD_ID;
		const bool withWdl = header.payloadId == WDL_PAYLOAD_ID || scoreCounts;
		const uint32_t countSize = scoreCounts ? SCORE_COUNT_PAYLOAD : WDL_PAYLOAD;
		if (withWdl && (header.payload < countSize || wdl.size() != nodes_.size())) {
			throw std::runtime_error("a book with counts needs " + std::to_string(countSize)
				+ " bytes of payload and a count for every node");
		}
		const size_t countAt = scoreCounts ? 4 : 2;
		const bool withRates = header.payloadId == RATE_PAYLOAD_ID;
		if (withRates && (header.payload < RATE_PAYLOAD || rates.size() != nodes_.size())) {
			throw std::runtime_error("a book with the rate payload needs 2 bytes and a rate for every node");
		}
		const bool combined = header.payloadId == COMBINED_PAYLOAD_ID;
		if (combined && (header.payload < COMBINED_PAYLOAD || rates.size() != nodes_.size())) {
			throw std::runtime_error("a book with score and rate needs 4 bytes and a rate for every node");
		}
		const auto put32 = [&](size_t at, uint32_t value) {
			for (int byte = 0; byte < 4; ++byte) data[at + byte] = uint8_t(value >> (8 * byte));
		};
		while (!stack.empty()) {
			const NodeIndex index = stack.back();
			const Node& node = nodes_[index];
			stack.pop_back();
			push(node.nextSibling);
			push(node.firstChild);
			const uint32_t bits = ((node.move & MOVE_MASK) << MOVE_SHIFT)
				| ((node.priority & PRIORITY_MASK) << PRIORITY_SHIFT)
				| (node.firstChild != NO_NODE ? HAS_CHILD : 0)
				| (node.nextSibling != NO_NODE ? HAS_RIGHT_SIBLING : 0);
			const size_t at = data.size();
			data.resize(at + nodeSize, 0);
			data[at] = uint8_t(bits);
			data[at + 1] = uint8_t(bits >> 8);
			if (combined) {
				data[at + 2] = uint8_t(uint16_t(node.score));
				data[at + 3] = uint8_t(uint16_t(node.score) >> 8);
				data[at + 4] = rates[index].rate;
				data[at + 5] = rates[index].significance;
			}
			else if (withRates) {
				data[at + 2] = rates[index].rate;
				data[at + 3] = rates[index].significance;
			}
			else if (withWdl) {
				if (scoreCounts) {
					data[at + 2] = uint8_t(uint16_t(node.score));
					data[at + 3] = uint8_t(uint16_t(node.score) >> 8);
				}
				put32(at + countAt, wdl[index].wins);
				put32(at + countAt + 4, wdl[index].draws);
				put32(at + countAt + 8, wdl[index].losses);
			}
			else if (header.payload >= VALUE_PAYLOAD && header.payloadId == SCORE_PAYLOAD_ID) {
				data[at + 2] = uint8_t(uint16_t(node.score));
				data[at + 3] = uint8_t(uint16_t(node.score) >> 8);
			}
			else if (header.payload >= VALUE_PAYLOAD) {
				data[at + 2] = uint8_t(node.value & MAX_VALUE_CODE);
				data[at + 3] = uint8_t((node.value & MAX_VALUE_CODE) >> 8);
			}
			if (data.size() >= (1 << 20)) {
				file.write(data.data(), data.size());
				data.clear();
			}
		}
		file.write(data.data(), data.size());
		file.close();
	}

	void Book::read(const std::string& path) {
		File file(path, "rb");
		header = BookHeader{};
		// A file without a version record is a book of the first generation.
		std::string fileVersion = "1";
		uint32_t nodeSize = 2;
		for (uint32_t tag = file.readInt("the header"); tag != END_OF_HEADER; tag = file.readInt("the header")) {
			const uint32_t length = file.readInt("the header");
			if (length > MAX_RECORD_LENGTH) {
				throw std::runtime_error(path + " is not a book: a header record of " + std::to_string(length) + " bytes");
			}
			std::string value(length, '\0');
			file.read(value.data(), length, "the header");
			// The stored string is zero terminated, the terminator is not part of it.
			if (!value.empty() && value.back() == '\0') value.pop_back();
			switch (tag) {
			case AUTHOR: header.author = value; break;
			case NAME: header.name = value; break;
			case BOOK_VERSION: header.version = value; break;
			case FILE_VERSION: fileVersion = value; break;
			case PAYLOAD_ID: header.payloadId = value; break;
			case NODE_SIZE: {
				const auto result = std::from_chars(value.data(), value.data() + value.size(), nodeSize);
				if (result.ec != std::errc() || nodeSize < 2 || nodeSize > 2 + MAX_PAYLOAD) {
					throw std::runtime_error(path + " is not a book: node size \"" + value + "\"");
				}
				break;
			}
			default: break;    // unknown records are skipped, that keeps the format extensible
			}
		}
		if (fileVersion != BOOK9_FILE_VERSION) {
			throw std::runtime_error(path + " has file version " + fileVersion + ", a 9 bit book has "
				+ BOOK9_FILE_VERSION);
		}
		header.payload = nodeSize - 2;
		const uint32_t count = file.readInt("the header");

		nodes_.assign(1, Node{});
		// Known from the header: growing the array instead would hold up to twice the book at once.
		nodes_.reserve(size_t(count) + 1);
		leaves_ = 0;
		const bool scoreCounts = header.payloadId == SCORE_COUNT_PAYLOAD_ID && header.payload >= SCORE_COUNT_PAYLOAD;
		const bool withWdl = scoreCounts || (header.payloadId == WDL_PAYLOAD_ID && header.payload >= WDL_PAYLOAD);
		const size_t countAt = scoreCounts ? 4 : 2;
		wdl.clear();
		if (withWdl) wdl.assign(size_t(count) + 1, WdlCount{});
		const bool combined = header.payloadId == COMBINED_PAYLOAD_ID && header.payload >= COMBINED_PAYLOAD;
		const bool withRates = combined || (header.payloadId == RATE_PAYLOAD_ID && header.payload >= RATE_PAYLOAD);
		const size_t rateAt = combined ? 4 : 2;
		rates.clear();
		if (withRates) rates.assign(size_t(count) + 1, WdlRate{});
		// One frame per level of the tree that is still being filled: the node the level hangs
		// below, and whether that node has a sibling of its own - which is what says how many levels
		// are finished once a node without a sibling is read.
		struct Frame {
			NodeIndex parent;
			bool parentHasSibling;
		};
		std::vector<Frame> frames{ Frame{ .parent = ROOT, .parentHasSibling = false } };
		std::vector<uint8_t> bytes(nodeSize);
		const auto get32 = [&](size_t at) {
			return uint32_t(bytes[at]) | (uint32_t(bytes[at + 1]) << 8) | (uint32_t(bytes[at + 2]) << 16)
				| (uint32_t(bytes[at + 3]) << 24);
		};
		for (uint32_t index = 0; index < count; ++index) {
			if (frames.empty()) throw std::runtime_error(path + " is not a book: nodes behind the tree");
			file.read(bytes.data(), nodeSize, "the nodes");
			const uint32_t bits = uint32_t(bytes[0]) | (uint32_t(bytes[1]) << 8);
			Node node{ .move = uint16_t((bits >> MOVE_SHIFT) & MOVE_MASK),
				.priority = uint8_t((bits >> PRIORITY_SHIFT) & PRIORITY_MASK) };
			if (header.payload >= VALUE_PAYLOAD && header.payloadId == VALUE_PAYLOAD_ID) {
				node.value = uint16_t((uint32_t(bytes[2]) | (uint32_t(bytes[3]) << 8)) & MAX_VALUE_CODE);
			}
			else if (header.payload >= VALUE_PAYLOAD && hasScores(header.payloadId)) {
				node.score = int16_t(uint16_t(bytes[2] | (bytes[3] << 8)));
			}
			const NodeIndex added = appendChild(frames.back().parent, node);
			if (withWdl) {
				wdl[added] = WdlCount{ .wins = get32(countAt), .draws = get32(countAt + 4),
					.losses = get32(countAt + 8) };
			}
			if (withRates) rates[added] = WdlRate{ .rate = bytes[rateAt], .significance = bytes[rateAt + 1] };
			if (bits & HAS_CHILD) {
				frames.push_back(Frame{ .parent = added, .parentHasSibling = (bits & HAS_RIGHT_SIBLING) != 0 });
			}
			else if (!(bits & HAS_RIGHT_SIBLING)) {
				for (bool levelIsDone = true; levelIsDone && !frames.empty();) {
					levelIsDone = !frames.back().parentHasSibling;
					frames.pop_back();
				}
			}
		}
		if (!frames.empty() && count != 0) {
			throw std::runtime_error(path + " is not a book: the tree ends inside a node's children");
		}
	}
}
