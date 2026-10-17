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
 *
 * A book evaluated by qapla-engine-tester --bookeval has the payload id SCORE_PAYLOAD_ID instead:
 * the first two bytes of a node hold, signed, the search score of the position its move is played
 * from, seen from the side to move there - centipawns from -30000 to 30000, 32000 - n for a mate in
 * n moves, -32000 + n for being mated in n, -32768 for no score.
 *
 * A book trained with games by gamefile --bookwdl has the payload id WDL_PAYLOAD_ID and twelve bytes
 * of payload: three unsigned 32 bit counts, little endian - the games that went through the node
 * and were won, drawn and lost by the side that plays its move, the side to move in the position
 * before it. A count stops at 2^32 - 1 rather than overflowing. The node is 14 bytes: an engine book
 * reads it with a payload type of 12 bytes and an alignment of at most two, uint16_t[6] or
 * uint8_t[12], as book9/book.h asks of every payload.
 *
 * Made from such a book by gamefile --wdl2rate, a book with the payload id RATE_PAYLOAD_ID carries
 * two bytes per node, see WdlRate: the rate of the games won by the side that plays the move, and
 * how many games that rate rests on.
 */

#include <cstdint>
#include <string>
#include <vector>

#include "game-file.h"

namespace GameFile {

	constexpr const char* BOOK9_FILE_VERSION = "9";
	/** The payload id of a book whose payload starts with the value code. */
	constexpr const char* VALUE_PAYLOAD_ID = "qapla-value-code-v1";
	/** The payload id of a book evaluated by qapla-engine-tester --bookeval. */
	constexpr const char* SCORE_PAYLOAD_ID = "qapla-search-score-v1";
	/** The search score of a book node, see SCORE_PAYLOAD_ID. */
	constexpr int16_t NO_SCORE = INT16_MIN;
	constexpr int32_t MAX_SCORE_CENTIPAWNS = 30000;
	constexpr int32_t SCORE_MATE = 32000;
	/** The payload id of a book trained with games by gamefile --bookwdl, see WdlCount. */
	constexpr const char* WDL_PAYLOAD_ID = "qapla-wdl-count-v1";
	constexpr uint32_t WDL_PAYLOAD = 12;

	/** The games through a book node, from the side that plays its move. */
	struct WdlCount {
		uint32_t wins = 0;
		uint32_t draws = 0;
		uint32_t losses = 0;
		bool operator==(const WdlCount&) const = default;
	};

	/** The payload id of a book made by gamefile --wdl2rate, see WdlRate. */
	constexpr const char* RATE_PAYLOAD_ID = "qapla-wdl-rate-v1";
	constexpr uint32_t RATE_PAYLOAD = 2;

	/**
	 * The counts of a node in two bytes.
	 *
	 * rate: the share of the points won by the side that plays the node's move, a draw counting
	 * half - (wins + draws / 2) / games - scaled to 0..255 and rounded half up. 0 and 255 mean
	 * exactly that: every game lost, every game won; every other share is kept within 1..254.
	 * significance: floor(log2 games) + 1 - 1 for one game, 2 for 2-3, 3 for 4-7, 4 for 8-15, 5 for
	 * 16-31 and so on; 0 for a node no game went through, whose rate is then 0 as well.
	 */
	struct WdlRate {
		uint8_t rate = 0;
		uint8_t significance = 0;
		bool operator==(const WdlRate&) const = default;
	};

	/** The rate and significance of the counts of a node. */
	constexpr WdlRate rateOf(const WdlCount& count) {
		const uint64_t games = uint64_t(count.wins) + count.draws + count.losses;
		if (games == 0) return WdlRate{};
		uint8_t significance = 0;
		for (uint64_t rest = games; rest != 0; rest >>= 1) ++significance;
		// Points in halves: a win two, a draw one, out of two per game.
		const uint64_t halves = 2 * uint64_t(count.wins) + count.draws;
		uint64_t rate;
		if (halves == 0) rate = 0;
		else if (halves == 2 * games) rate = 255;
		else {
			// halves / (2 games) * 255, rounded half up, kept away from the two ends.
			rate = (halves * 255 + games) / (2 * games);
			rate = rate < 1 ? 1 : (rate > 254 ? 254 : rate);
		}
		return WdlRate{ .rate = uint8_t(rate), .significance = significance };
	}

	/** The value pgn2gam reads a mate as, in the unit of the engine. */
	constexpr int MATE_VALUE = 30000;

	/** The value of a search score in the unit of the engine, as pgn2gam reads it from a comment. */
	constexpr int valueOfScore(int16_t score) {
		if (score > MAX_SCORE_CENTIPAWNS) return MATE_VALUE;
		if (score < -MAX_SCORE_CENTIPAWNS) return -MATE_VALUE;
		return score;
	}
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

		/** Kept small: a book of the training data has many millions of nodes. */
		struct Node {
			NodeIndex firstChild = NO_NODE;
			NodeIndex lastChild = NO_NODE;
			NodeIndex nextSibling = NO_NODE;
			uint16_t move = 0;
			/** The value code of the move, NO_GAME_VALUE for none. */
			uint16_t value = NO_GAME_VALUE;
			/** The search score of the position the move is played from, NO_SCORE for none. */
			int16_t score = NO_SCORE;
			uint8_t priority = PRIORITY_DEFAULT;
		};

		Book();

		/** The child of a node with this nine bit move, added behind its siblings if it is new. */
		NodeIndex addMove(NodeIndex parent, uint32_t move);

		/** The child of a node with this nine bit move, NO_NODE if the book has none. */
		NodeIndex findMove(NodeIndex parent, uint32_t move) const;

		const Node& node(NodeIndex index) const { return nodes_[index]; }
		Node& node(NodeIndex index) { return nodes_[index]; }

		/** The moves of the book, the root not counted. */
		size_t size() const { return nodes_.size() - 1; }
		size_t leafCount() const { return leaves_; }

		BookHeader header;

		/**
		 * The counts of a book with the WDL payload, by node index; empty for every other book. Kept
		 * apart from the nodes, which a book without them should not pay for.
		 */
		std::vector<WdlCount> wdl;

		/** The rates of a book with the rate payload, by node index; empty for every other book. */
		std::vector<WdlRate> rates;

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
