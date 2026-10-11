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
#pragma once

/**
 * The 9 bit book of the Qapla engine (src/book9/book.h, file version "9"): a tree of moves, every
 * node with a payload of a size the file states.
 *
 * The header is the one of all Qapla and Spike books (src/book/book-io.h): typed records, each a
 * 32 bit tag and a 32 bit length followed by a zero terminated string, ended by a tag of zero and
 * followed by the number of nodes. All integers are little endian. The records written are author,
 * name, book version, file version "9", payload id and node size; a reader skips the ones it does
 * not know.
 *
 * The nodes follow in preorder: the first child of a node right behind it, its right sibling behind
 * its whole subtree. A node is two bytes, little endian, and then its payload:
 *   (msb) MMMM MMMM MPPP PPSC (lsb)
 *   M = the nine bit move of src/book9/packed-move.h, played from the position of the parent
 *   P = priority, 0 to 31: the higher the more often it is played, 0 never; 16 by default
 *   S = the node has a right sibling, C = the node has a child
 * The node size record is 2 plus the payload. With a payload of two bytes or more the first two hold
 * the value code of the move, as in a game file, and the payload id says so; the bytes behind it
 * are free and written as zeros. Without a payload the file is byte for byte the book an engine
 * book of EmptyPayload writes.
 */

#include <cstdint>
#include <string>
#include <vector>

#include "game-file.h"

namespace GameFile {

	constexpr const char* BOOK9_FILE_VERSION = "9";
	/** The payload id of a book whose payload starts with the value code. */
	constexpr const char* VALUE_PAYLOAD_ID = "qapla-value-code-v1";
	constexpr uint32_t PRIORITY_DEFAULT = 16;
	constexpr uint32_t PRIORITY_MAX = 31;

	struct BookHeader {
		std::string author;
		std::string name;
		std::string version;
		std::string payloadId;
		/** Bytes behind the two bytes of the move, the node size minus two. */
		uint32_t payload = 0;
	};

	/** A 9 bit book in memory: the tree with explicit links, the file holds it in preorder. */
	class Book {
	public:
		using NodeIndex = uint32_t;
		/** The position before the first move; it has no move and is not stored. */
		static constexpr NodeIndex ROOT = 0;
		static constexpr NodeIndex NO_NODE = ~NodeIndex(0);

		struct Node {
			uint32_t move = 0;
			uint32_t priority = PRIORITY_DEFAULT;
			/** The value code of the move, NO_GAME_VALUE for none. */
			uint32_t value = NO_GAME_VALUE;
			NodeIndex firstChild = NO_NODE;
			NodeIndex lastChild = NO_NODE;
			NodeIndex nextSibling = NO_NODE;
		};

		Book();

		/** The child of a node with this nine bit move, added behind its siblings if it is new. */
		NodeIndex addMove(NodeIndex parent, uint32_t move);

		const Node& node(NodeIndex index) const { return nodes_[index]; }
		Node& node(NodeIndex index) { return nodes_[index]; }

		/** The moves of the book, the root not counted. */
		size_t size() const { return nodes_.size() - 1; }
		size_t leafCount() const { return leaves_; }

		BookHeader header;

		/** @throws std::runtime_error if the file cannot be written. */
		void write(const std::string& path) const;

		/**
		 * Replaces the book by the one of the file.
		 * @throws std::runtime_error if the file cannot be read or is not a 9 bit book.
		 */
		void read(const std::string& path);

	private:
		NodeIndex appendChild(NodeIndex parent, const Node& node);

		std::vector<Node> nodes_;
		size_t leaves_ = 0;
	};
}
