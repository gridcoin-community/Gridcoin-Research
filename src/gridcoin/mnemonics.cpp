// Copyright (c) 2024-2025 The Gridcoin developers
// Distributed under the MIT/X11 software license, see the accompanying
// file COPYING or https://opensource.org/licenses/mit-license.php.

#include <gridcoin/mnemonics.h>

#include <crypto/chacha20.h>
#include <crypto/chacha20poly1305.h>
#include <crypto/scrypt.h>
#include <crypto/sha256.h>
#include <key.h>
#include <random.h>
#include <span.h>
#include <support/cleanse.h>
#include <util/time.h>

#include <algorithm>
#include <cassert>
#include <cstring>
#include <iterator>

using namespace GRC::Mnemonics;

namespace {

//! Length of the longest word in the wordlist.
constexpr unsigned LONGEST_WORD_LENGTH = 8;

//! The BIP39 English wordlist, used here purely as a well-vetted encoding
//! alphabet (2048 words; no word is a prefix of another within the first four
//! letters; edit-distance screened). The phrase payload is NOT
//! BIP39-compatible. Verified against the canonical english.txt
//! (SHA256 2f5eed53a4727b4bf8880d8f3f199efc90e58503646d9ff8eff3a2ed3b24dbda).
constexpr const char* WORDLIST[2048] = {
    "abandon", "ability", "able", "about", "above", "absent", "absorb", "abstract",
    "absurd", "abuse", "access", "accident", "account", "accuse", "achieve", "acid",
    "acoustic", "acquire", "across", "act", "action", "actor", "actress", "actual",
    "adapt", "add", "addict", "address", "adjust", "admit", "adult", "advance",
    "advice", "aerobic", "affair", "afford", "afraid", "again", "age", "agent",
    "agree", "ahead", "aim", "air", "airport", "aisle", "alarm", "album",
    "alcohol", "alert", "alien", "all", "alley", "allow", "almost", "alone",
    "alpha", "already", "also", "alter", "always", "amateur", "amazing", "among",
    "amount", "amused", "analyst", "anchor", "ancient", "anger", "angle", "angry",
    "animal", "ankle", "announce", "annual", "another", "answer", "antenna", "antique",
    "anxiety", "any", "apart", "apology", "appear", "apple", "approve", "april",
    "arch", "arctic", "area", "arena", "argue", "arm", "armed", "armor",
    "army", "around", "arrange", "arrest", "arrive", "arrow", "art", "artefact",
    "artist", "artwork", "ask", "aspect", "assault", "asset", "assist", "assume",
    "asthma", "athlete", "atom", "attack", "attend", "attitude", "attract", "auction",
    "audit", "august", "aunt", "author", "auto", "autumn", "average", "avocado",
    "avoid", "awake", "aware", "away", "awesome", "awful", "awkward", "axis",
    "baby", "bachelor", "bacon", "badge", "bag", "balance", "balcony", "ball",
    "bamboo", "banana", "banner", "bar", "barely", "bargain", "barrel", "base",
    "basic", "basket", "battle", "beach", "bean", "beauty", "because", "become",
    "beef", "before", "begin", "behave", "behind", "believe", "below", "belt",
    "bench", "benefit", "best", "betray", "better", "between", "beyond", "bicycle",
    "bid", "bike", "bind", "biology", "bird", "birth", "bitter", "black",
    "blade", "blame", "blanket", "blast", "bleak", "bless", "blind", "blood",
    "blossom", "blouse", "blue", "blur", "blush", "board", "boat", "body",
    "boil", "bomb", "bone", "bonus", "book", "boost", "border", "boring",
    "borrow", "boss", "bottom", "bounce", "box", "boy", "bracket", "brain",
    "brand", "brass", "brave", "bread", "breeze", "brick", "bridge", "brief",
    "bright", "bring", "brisk", "broccoli", "broken", "bronze", "broom", "brother",
    "brown", "brush", "bubble", "buddy", "budget", "buffalo", "build", "bulb",
    "bulk", "bullet", "bundle", "bunker", "burden", "burger", "burst", "bus",
    "business", "busy", "butter", "buyer", "buzz", "cabbage", "cabin", "cable",
    "cactus", "cage", "cake", "call", "calm", "camera", "camp", "can",
    "canal", "cancel", "candy", "cannon", "canoe", "canvas", "canyon", "capable",
    "capital", "captain", "car", "carbon", "card", "cargo", "carpet", "carry",
    "cart", "case", "cash", "casino", "castle", "casual", "cat", "catalog",
    "catch", "category", "cattle", "caught", "cause", "caution", "cave", "ceiling",
    "celery", "cement", "census", "century", "cereal", "certain", "chair", "chalk",
    "champion", "change", "chaos", "chapter", "charge", "chase", "chat", "cheap",
    "check", "cheese", "chef", "cherry", "chest", "chicken", "chief", "child",
    "chimney", "choice", "choose", "chronic", "chuckle", "chunk", "churn", "cigar",
    "cinnamon", "circle", "citizen", "city", "civil", "claim", "clap", "clarify",
    "claw", "clay", "clean", "clerk", "clever", "click", "client", "cliff",
    "climb", "clinic", "clip", "clock", "clog", "close", "cloth", "cloud",
    "clown", "club", "clump", "cluster", "clutch", "coach", "coast", "coconut",
    "code", "coffee", "coil", "coin", "collect", "color", "column", "combine",
    "come", "comfort", "comic", "common", "company", "concert", "conduct", "confirm",
    "congress", "connect", "consider", "control", "convince", "cook", "cool", "copper",
    "copy", "coral", "core", "corn", "correct", "cost", "cotton", "couch",
    "country", "couple", "course", "cousin", "cover", "coyote", "crack", "cradle",
    "craft", "cram", "crane", "crash", "crater", "crawl", "crazy", "cream",
    "credit", "creek", "crew", "cricket", "crime", "crisp", "critic", "crop",
    "cross", "crouch", "crowd", "crucial", "cruel", "cruise", "crumble", "crunch",
    "crush", "cry", "crystal", "cube", "culture", "cup", "cupboard", "curious",
    "current", "curtain", "curve", "cushion", "custom", "cute", "cycle", "dad",
    "damage", "damp", "dance", "danger", "daring", "dash", "daughter", "dawn",
    "day", "deal", "debate", "debris", "decade", "december", "decide", "decline",
    "decorate", "decrease", "deer", "defense", "define", "defy", "degree", "delay",
    "deliver", "demand", "demise", "denial", "dentist", "deny", "depart", "depend",
    "deposit", "depth", "deputy", "derive", "describe", "desert", "design", "desk",
    "despair", "destroy", "detail", "detect", "develop", "device", "devote", "diagram",
    "dial", "diamond", "diary", "dice", "diesel", "diet", "differ", "digital",
    "dignity", "dilemma", "dinner", "dinosaur", "direct", "dirt", "disagree", "discover",
    "disease", "dish", "dismiss", "disorder", "display", "distance", "divert", "divide",
    "divorce", "dizzy", "doctor", "document", "dog", "doll", "dolphin", "domain",
    "donate", "donkey", "donor", "door", "dose", "double", "dove", "draft",
    "dragon", "drama", "drastic", "draw", "dream", "dress", "drift", "drill",
    "drink", "drip", "drive", "drop", "drum", "dry", "duck", "dumb",
    "dune", "during", "dust", "dutch", "duty", "dwarf", "dynamic", "eager",
    "eagle", "early", "earn", "earth", "easily", "east", "easy", "echo",
    "ecology", "economy", "edge", "edit", "educate", "effort", "egg", "eight",
    "either", "elbow", "elder", "electric", "elegant", "element", "elephant", "elevator",
    "elite", "else", "embark", "embody", "embrace", "emerge", "emotion", "employ",
    "empower", "empty", "enable", "enact", "end", "endless", "endorse", "enemy",
    "energy", "enforce", "engage", "engine", "enhance", "enjoy", "enlist", "enough",
    "enrich", "enroll", "ensure", "enter", "entire", "entry", "envelope", "episode",
    "equal", "equip", "era", "erase", "erode", "erosion", "error", "erupt",
    "escape", "essay", "essence", "estate", "eternal", "ethics", "evidence", "evil",
    "evoke", "evolve", "exact", "example", "excess", "exchange", "excite", "exclude",
    "excuse", "execute", "exercise", "exhaust", "exhibit", "exile", "exist", "exit",
    "exotic", "expand", "expect", "expire", "explain", "expose", "express", "extend",
    "extra", "eye", "eyebrow", "fabric", "face", "faculty", "fade", "faint",
    "faith", "fall", "false", "fame", "family", "famous", "fan", "fancy",
    "fantasy", "farm", "fashion", "fat", "fatal", "father", "fatigue", "fault",
    "favorite", "feature", "february", "federal", "fee", "feed", "feel", "female",
    "fence", "festival", "fetch", "fever", "few", "fiber", "fiction", "field",
    "figure", "file", "film", "filter", "final", "find", "fine", "finger",
    "finish", "fire", "firm", "first", "fiscal", "fish", "fit", "fitness",
    "fix", "flag", "flame", "flash", "flat", "flavor", "flee", "flight",
    "flip", "float", "flock", "floor", "flower", "fluid", "flush", "fly",
    "foam", "focus", "fog", "foil", "fold", "follow", "food", "foot",
    "force", "forest", "forget", "fork", "fortune", "forum", "forward", "fossil",
    "foster", "found", "fox", "fragile", "frame", "frequent", "fresh", "friend",
    "fringe", "frog", "front", "frost", "frown", "frozen", "fruit", "fuel",
    "fun", "funny", "furnace", "fury", "future", "gadget", "gain", "galaxy",
    "gallery", "game", "gap", "garage", "garbage", "garden", "garlic", "garment",
    "gas", "gasp", "gate", "gather", "gauge", "gaze", "general", "genius",
    "genre", "gentle", "genuine", "gesture", "ghost", "giant", "gift", "giggle",
    "ginger", "giraffe", "girl", "give", "glad", "glance", "glare", "glass",
    "glide", "glimpse", "globe", "gloom", "glory", "glove", "glow", "glue",
    "goat", "goddess", "gold", "good", "goose", "gorilla", "gospel", "gossip",
    "govern", "gown", "grab", "grace", "grain", "grant", "grape", "grass",
    "gravity", "great", "green", "grid", "grief", "grit", "grocery", "group",
    "grow", "grunt", "guard", "guess", "guide", "guilt", "guitar", "gun",
    "gym", "habit", "hair", "half", "hammer", "hamster", "hand", "happy",
    "harbor", "hard", "harsh", "harvest", "hat", "have", "hawk", "hazard",
    "head", "health", "heart", "heavy", "hedgehog", "height", "hello", "helmet",
    "help", "hen", "hero", "hidden", "high", "hill", "hint", "hip",
    "hire", "history", "hobby", "hockey", "hold", "hole", "holiday", "hollow",
    "home", "honey", "hood", "hope", "horn", "horror", "horse", "hospital",
    "host", "hotel", "hour", "hover", "hub", "huge", "human", "humble",
    "humor", "hundred", "hungry", "hunt", "hurdle", "hurry", "hurt", "husband",
    "hybrid", "ice", "icon", "idea", "identify", "idle", "ignore", "ill",
    "illegal", "illness", "image", "imitate", "immense", "immune", "impact", "impose",
    "improve", "impulse", "inch", "include", "income", "increase", "index", "indicate",
    "indoor", "industry", "infant", "inflict", "inform", "inhale", "inherit", "initial",
    "inject", "injury", "inmate", "inner", "innocent", "input", "inquiry", "insane",
    "insect", "inside", "inspire", "install", "intact", "interest", "into", "invest",
    "invite", "involve", "iron", "island", "isolate", "issue", "item", "ivory",
    "jacket", "jaguar", "jar", "jazz", "jealous", "jeans", "jelly", "jewel",
    "job", "join", "joke", "journey", "joy", "judge", "juice", "jump",
    "jungle", "junior", "junk", "just", "kangaroo", "keen", "keep", "ketchup",
    "key", "kick", "kid", "kidney", "kind", "kingdom", "kiss", "kit",
    "kitchen", "kite", "kitten", "kiwi", "knee", "knife", "knock", "know",
    "lab", "label", "labor", "ladder", "lady", "lake", "lamp", "language",
    "laptop", "large", "later", "latin", "laugh", "laundry", "lava", "law",
    "lawn", "lawsuit", "layer", "lazy", "leader", "leaf", "learn", "leave",
    "lecture", "left", "leg", "legal", "legend", "leisure", "lemon", "lend",
    "length", "lens", "leopard", "lesson", "letter", "level", "liar", "liberty",
    "library", "license", "life", "lift", "light", "like", "limb", "limit",
    "link", "lion", "liquid", "list", "little", "live", "lizard", "load",
    "loan", "lobster", "local", "lock", "logic", "lonely", "long", "loop",
    "lottery", "loud", "lounge", "love", "loyal", "lucky", "luggage", "lumber",
    "lunar", "lunch", "luxury", "lyrics", "machine", "mad", "magic", "magnet",
    "maid", "mail", "main", "major", "make", "mammal", "man", "manage",
    "mandate", "mango", "mansion", "manual", "maple", "marble", "march", "margin",
    "marine", "market", "marriage", "mask", "mass", "master", "match", "material",
    "math", "matrix", "matter", "maximum", "maze", "meadow", "mean", "measure",
    "meat", "mechanic", "medal", "media", "melody", "melt", "member", "memory",
    "mention", "menu", "mercy", "merge", "merit", "merry", "mesh", "message",
    "metal", "method", "middle", "midnight", "milk", "million", "mimic", "mind",
    "minimum", "minor", "minute", "miracle", "mirror", "misery", "miss", "mistake",
    "mix", "mixed", "mixture", "mobile", "model", "modify", "mom", "moment",
    "monitor", "monkey", "monster", "month", "moon", "moral", "more", "morning",
    "mosquito", "mother", "motion", "motor", "mountain", "mouse", "move", "movie",
    "much", "muffin", "mule", "multiply", "muscle", "museum", "mushroom", "music",
    "must", "mutual", "myself", "mystery", "myth", "naive", "name", "napkin",
    "narrow", "nasty", "nation", "nature", "near", "neck", "need", "negative",
    "neglect", "neither", "nephew", "nerve", "nest", "net", "network", "neutral",
    "never", "news", "next", "nice", "night", "noble", "noise", "nominee",
    "noodle", "normal", "north", "nose", "notable", "note", "nothing", "notice",
    "novel", "now", "nuclear", "number", "nurse", "nut", "oak", "obey",
    "object", "oblige", "obscure", "observe", "obtain", "obvious", "occur", "ocean",
    "october", "odor", "off", "offer", "office", "often", "oil", "okay",
    "old", "olive", "olympic", "omit", "once", "one", "onion", "online",
    "only", "open", "opera", "opinion", "oppose", "option", "orange", "orbit",
    "orchard", "order", "ordinary", "organ", "orient", "original", "orphan", "ostrich",
    "other", "outdoor", "outer", "output", "outside", "oval", "oven", "over",
    "own", "owner", "oxygen", "oyster", "ozone", "pact", "paddle", "page",
    "pair", "palace", "palm", "panda", "panel", "panic", "panther", "paper",
    "parade", "parent", "park", "parrot", "party", "pass", "patch", "path",
    "patient", "patrol", "pattern", "pause", "pave", "payment", "peace", "peanut",
    "pear", "peasant", "pelican", "pen", "penalty", "pencil", "people", "pepper",
    "perfect", "permit", "person", "pet", "phone", "photo", "phrase", "physical",
    "piano", "picnic", "picture", "piece", "pig", "pigeon", "pill", "pilot",
    "pink", "pioneer", "pipe", "pistol", "pitch", "pizza", "place", "planet",
    "plastic", "plate", "play", "please", "pledge", "pluck", "plug", "plunge",
    "poem", "poet", "point", "polar", "pole", "police", "pond", "pony",
    "pool", "popular", "portion", "position", "possible", "post", "potato", "pottery",
    "poverty", "powder", "power", "practice", "praise", "predict", "prefer", "prepare",
    "present", "pretty", "prevent", "price", "pride", "primary", "print", "priority",
    "prison", "private", "prize", "problem", "process", "produce", "profit", "program",
    "project", "promote", "proof", "property", "prosper", "protect", "proud", "provide",
    "public", "pudding", "pull", "pulp", "pulse", "pumpkin", "punch", "pupil",
    "puppy", "purchase", "purity", "purpose", "purse", "push", "put", "puzzle",
    "pyramid", "quality", "quantum", "quarter", "question", "quick", "quit", "quiz",
    "quote", "rabbit", "raccoon", "race", "rack", "radar", "radio", "rail",
    "rain", "raise", "rally", "ramp", "ranch", "random", "range", "rapid",
    "rare", "rate", "rather", "raven", "raw", "razor", "ready", "real",
    "reason", "rebel", "rebuild", "recall", "receive", "recipe", "record", "recycle",
    "reduce", "reflect", "reform", "refuse", "region", "regret", "regular", "reject",
    "relax", "release", "relief", "rely", "remain", "remember", "remind", "remove",
    "render", "renew", "rent", "reopen", "repair", "repeat", "replace", "report",
    "require", "rescue", "resemble", "resist", "resource", "response", "result", "retire",
    "retreat", "return", "reunion", "reveal", "review", "reward", "rhythm", "rib",
    "ribbon", "rice", "rich", "ride", "ridge", "rifle", "right", "rigid",
    "ring", "riot", "ripple", "risk", "ritual", "rival", "river", "road",
    "roast", "robot", "robust", "rocket", "romance", "roof", "rookie", "room",
    "rose", "rotate", "rough", "round", "route", "royal", "rubber", "rude",
    "rug", "rule", "run", "runway", "rural", "sad", "saddle", "sadness",
    "safe", "sail", "salad", "salmon", "salon", "salt", "salute", "same",
    "sample", "sand", "satisfy", "satoshi", "sauce", "sausage", "save", "say",
    "scale", "scan", "scare", "scatter", "scene", "scheme", "school", "science",
    "scissors", "scorpion", "scout", "scrap", "screen", "script", "scrub", "sea",
    "search", "season", "seat", "second", "secret", "section", "security", "seed",
    "seek", "segment", "select", "sell", "seminar", "senior", "sense", "sentence",
    "series", "service", "session", "settle", "setup", "seven", "shadow", "shaft",
    "shallow", "share", "shed", "shell", "sheriff", "shield", "shift", "shine",
    "ship", "shiver", "shock", "shoe", "shoot", "shop", "short", "shoulder",
    "shove", "shrimp", "shrug", "shuffle", "shy", "sibling", "sick", "side",
    "siege", "sight", "sign", "silent", "silk", "silly", "silver", "similar",
    "simple", "since", "sing", "siren", "sister", "situate", "six", "size",
    "skate", "sketch", "ski", "skill", "skin", "skirt", "skull", "slab",
    "slam", "sleep", "slender", "slice", "slide", "slight", "slim", "slogan",
    "slot", "slow", "slush", "small", "smart", "smile", "smoke", "smooth",
    "snack", "snake", "snap", "sniff", "snow", "soap", "soccer", "social",
    "sock", "soda", "soft", "solar", "soldier", "solid", "solution", "solve",
    "someone", "song", "soon", "sorry", "sort", "soul", "sound", "soup",
    "source", "south", "space", "spare", "spatial", "spawn", "speak", "special",
    "speed", "spell", "spend", "sphere", "spice", "spider", "spike", "spin",
    "spirit", "split", "spoil", "sponsor", "spoon", "sport", "spot", "spray",
    "spread", "spring", "spy", "square", "squeeze", "squirrel", "stable", "stadium",
    "staff", "stage", "stairs", "stamp", "stand", "start", "state", "stay",
    "steak", "steel", "stem", "step", "stereo", "stick", "still", "sting",
    "stock", "stomach", "stone", "stool", "story", "stove", "strategy", "street",
    "strike", "strong", "struggle", "student", "stuff", "stumble", "style", "subject",
    "submit", "subway", "success", "such", "sudden", "suffer", "sugar", "suggest",
    "suit", "summer", "sun", "sunny", "sunset", "super", "supply", "supreme",
    "sure", "surface", "surge", "surprise", "surround", "survey", "suspect", "sustain",
    "swallow", "swamp", "swap", "swarm", "swear", "sweet", "swift", "swim",
    "swing", "switch", "sword", "symbol", "symptom", "syrup", "system", "table",
    "tackle", "tag", "tail", "talent", "talk", "tank", "tape", "target",
    "task", "taste", "tattoo", "taxi", "teach", "team", "tell", "ten",
    "tenant", "tennis", "tent", "term", "test", "text", "thank", "that",
    "theme", "then", "theory", "there", "they", "thing", "this", "thought",
    "three", "thrive", "throw", "thumb", "thunder", "ticket", "tide", "tiger",
    "tilt", "timber", "time", "tiny", "tip", "tired", "tissue", "title",
    "toast", "tobacco", "today", "toddler", "toe", "together", "toilet", "token",
    "tomato", "tomorrow", "tone", "tongue", "tonight", "tool", "tooth", "top",
    "topic", "topple", "torch", "tornado", "tortoise", "toss", "total", "tourist",
    "toward", "tower", "town", "toy", "track", "trade", "traffic", "tragic",
    "train", "transfer", "trap", "trash", "travel", "tray", "treat", "tree",
    "trend", "trial", "tribe", "trick", "trigger", "trim", "trip", "trophy",
    "trouble", "truck", "true", "truly", "trumpet", "trust", "truth", "try",
    "tube", "tuition", "tumble", "tuna", "tunnel", "turkey", "turn", "turtle",
    "twelve", "twenty", "twice", "twin", "twist", "two", "type", "typical",
    "ugly", "umbrella", "unable", "unaware", "uncle", "uncover", "under", "undo",
    "unfair", "unfold", "unhappy", "uniform", "unique", "unit", "universe", "unknown",
    "unlock", "until", "unusual", "unveil", "update", "upgrade", "uphold", "upon",
    "upper", "upset", "urban", "urge", "usage", "use", "used", "useful",
    "useless", "usual", "utility", "vacant", "vacuum", "vague", "valid", "valley",
    "valve", "van", "vanish", "vapor", "various", "vast", "vault", "vehicle",
    "velvet", "vendor", "venture", "venue", "verb", "verify", "version", "very",
    "vessel", "veteran", "viable", "vibrant", "vicious", "victory", "video", "view",
    "village", "vintage", "violin", "virtual", "virus", "visa", "visit", "visual",
    "vital", "vivid", "vocal", "voice", "void", "volcano", "volume", "vote",
    "voyage", "wage", "wagon", "wait", "walk", "wall", "walnut", "want",
    "warfare", "warm", "warrior", "wash", "wasp", "waste", "water", "wave",
    "way", "wealth", "weapon", "wear", "weasel", "weather", "web", "wedding",
    "weekend", "weird", "welcome", "west", "wet", "whale", "what", "wheat",
    "wheel", "when", "where", "whip", "whisper", "wide", "width", "wife",
    "wild", "will", "win", "window", "wine", "wing", "wink", "winner",
    "winter", "wire", "wisdom", "wise", "wish", "witness", "wolf", "woman",
    "wonder", "wood", "wool", "word", "work", "world", "worry", "worth",
    "wrap", "wreck", "wrestle", "wrist", "write", "wrong", "yard", "year",
    "yellow", "you", "young", "youth", "zebra", "zero", "zone", "zoo"
};

static_assert((1u << WORDLIST_BIT_LENGTH) == std::size(WORDLIST));

//! LONGEST_WORD_LENGTH is load-bearing: it sizes the word-selection buffer in
//! EncodeSeedPhrase and bounds the separator scan window. Pin it against the
//! actual wordlist at compile time.
constexpr bool WordlistLengthsValid()
{
    for (unsigned i = 0; i < std::size(WORDLIST); ++i) {
        unsigned length = 0;
        while (WORDLIST[i][length] != '\0') {
            ++length;
        }
        if (length == 0 || length > LONGEST_WORD_LENGTH) {
            return false;
        }
    }
    return true;
}
static_assert(WordlistLengthsValid(),
              "every wordlist entry must be between 1 and LONGEST_WORD_LENGTH characters");

//! Timing attack safe word to index mapper. Returns -1 if the word is not found.
int timingsafe_wordfind(const SecureString& phrase, int word_start, int word_end)
{
    int ret = -1;
    const int word_length = word_end - word_start;
    const int last_char_index = std::min(word_end - 1, (int)phrase.size() - 1);

    for (int i = 0; i < (int)std::size(WORDLIST); ++i) {
        const int other_last_char_index = (int)strlen(WORDLIST[i]) - 1;
        bool found = word_length == other_last_char_index + 1;

        for (int j = 0; j < (int)LONGEST_WORD_LENGTH + 1; ++j) {
            found &= (phrase[std::min(word_start + j, last_char_index)]
                      == WORDLIST[i][std::min(j, other_last_char_index)]);
        }

        found &= (ret == -1);
        ret = found * i + (1 - found) * ret;
    }

    return ret;
}

//! Timing attack safe space finder for splitting the words. Points to the end if not found.
int timingsafe_spacefind(const SecureString& phrase, int start)
{
    int ret = -1;
    const int last_char_index = phrase.size() - 1;

    for (int i = start; i < start + (int)LONGEST_WORD_LENGTH + 1; ++i) {
        bool is_whitespace = (phrase[std::min(i, last_char_index)] == ' ');
        is_whitespace &= (ret == -1);
        ret = is_whitespace * i + (1 - is_whitespace) * ret;
    }

    const bool found = ret != -1;
    return found * ret + (1 - found) * (int)phrase.size();
}

//! Constant-time byte comparison. Returns 0 when equal.
int timingsafe_bcmp(const std::byte* b1, const std::byte* b2, size_t n)
{
    unsigned char ret = 0;
    for (; n > 0; --n) {
        ret |= std::to_integer<unsigned char>(*b1++) ^ std::to_integer<unsigned char>(*b2++);
    }
    return ret != 0;
}

//! Read the 11-bit word index at word position `word`. Bit 0 is the most
//! significant bit of the first byte.
unsigned GetWordBits(Span<const std::byte> data, unsigned word)
{
    unsigned index = 0;
    for (unsigned b = 0; b < WORDLIST_BIT_LENGTH; ++b) {
        const unsigned bit = word * WORDLIST_BIT_LENGTH + b;
        index = (index << 1) | ((std::to_integer<unsigned>(data[bit / 8]) >> (7 - bit % 8)) & 1u);
    }
    return index;
}

//! Write the 11-bit word index at word position `word`. The destination must
//! start zeroed. Branchless: no secret-dependent control flow.
void SetWordBits(Span<std::byte> data, unsigned word, unsigned index)
{
    for (unsigned b = 0; b < WORDLIST_BIT_LENGTH; ++b) {
        const unsigned bit = word * WORDLIST_BIT_LENGTH + b;
        const unsigned value = (index >> (WORDLIST_BIT_LENGTH - 1 - b)) & 1u;
        data[bit / 8] |= std::byte(value << (7 - bit % 8));
    }
}

//! Derive the AEAD key from the password and salt with scrypt.
void DeriveKey(const SecureString& password, Span<const std::byte> salt, Span<std::byte> key_out)
{
    ScryptRFC7914(Span{reinterpret_cast<const std::byte*>(password.data()), password.size()},
                  salt, SCRYPT_N, SCRYPT_R, SCRYPT_P, key_out);
}

//! Derive the wallet key from the entropy (inner version 0).
bool DeriveWalletKey(Span<const std::byte> entropy, CKey& key_out)
{
    unsigned char key_data[CSHA256::OUTPUT_SIZE];
    CSHA256().Write(UCharCast(entropy.data()), entropy.size()).Finalize(key_data);
    key_out.Set(std::begin(key_data), std::end(key_data), /*fCompressedIn=*/true);
    memory_cleanse(key_data, sizeof(key_data));
    return key_out.IsValid();
}

} // anonymous namespace

bool GRC::Mnemonics::DecodeSeedPhrase(const SecureString& seed_phrase, Span<std::byte> data_out)
{
    assert(data_out.size() == ENCIPHERED_LENGTH);
    std::fill(data_out.begin(), data_out.end(), std::byte{0});

    // No valid phrase exceeds WORD_COUNT longest words plus separators (one
    // trailing separator tolerated). Rejecting oversized input up front also
    // keeps the int index arithmetic in the scanners trivially in range.
    if (seed_phrase.empty()
        || seed_phrase.size() > WORD_COUNT * (LONGEST_WORD_LENGTH + 1)) {
        return false;
    }

    unsigned word_count = 0;
    SecureString::size_type word_start = 0;
    do {
        const int word_end = timingsafe_spacefind(seed_phrase, word_start);

        // An empty segment (leading, doubled or misplaced separator): reject
        // before the word scan, which requires a non-empty candidate.
        if (word_end == (int)word_start) {
            std::fill(data_out.begin(), data_out.end(), std::byte{0});
            return false;
        }

        const int index = timingsafe_wordfind(seed_phrase, word_start, word_end);

        if (index == -1 || word_count >= WORD_COUNT) {
            std::fill(data_out.begin(), data_out.end(), std::byte{0});
            return false;
        }

        SetWordBits(data_out, word_count, index);

        word_start = word_end + 1;
        ++word_count;
    } while (word_start < seed_phrase.size());

    if (word_count != WORD_COUNT) {
        std::fill(data_out.begin(), data_out.end(), std::byte{0});
        return false;
    }

    return true;
}

SecureString GRC::Mnemonics::EncodeSeedPhrase(Span<const std::byte> data_in)
{
    assert(data_in.size() == ENCIPHERED_LENGTH);

    SecureString seed_phrase;
    seed_phrase.reserve(WORD_COUNT * (LONGEST_WORD_LENGTH + 1));

    for (unsigned i = 0; i < WORD_COUNT; ++i) {
        const unsigned index = GetWordBits(data_in, i);

        // Select the word by scanning the whole (public) table with branchless
        // accumulation, so the secret index is not exposed through a single
        // indexed load.
        char buffer[LONGEST_WORD_LENGTH] = {};
        int length = 0;
        for (unsigned w = 0; w < std::size(WORDLIST); ++w) {
            const int selected = (w == index);
            const int word_length = (int)strlen(WORDLIST[w]);
            for (int j = 0; j < (int)LONGEST_WORD_LENGTH; ++j) {
                const char c = WORDLIST[w][std::min(j, word_length - 1)];
                buffer[j] = (char)(selected * c + (1 - selected) * buffer[j]);
            }
            length = selected * word_length + (1 - selected) * length;
        }

        if (i != 0) {
            seed_phrase += ' ';
        }
        seed_phrase.append(buffer, length);
        memory_cleanse(buffer, sizeof(buffer));
    }

    return seed_phrase;
}

SecureString GRC::Mnemonics::BuildSeedPhrase(Span<const std::byte> entropy, uint16_t birthday_days,
                                             Span<const std::byte> salt, const SecureString& password)
{
    assert(entropy.size() == ENTROPY_LENGTH);
    assert(salt.size() == SALT_LENGTH);

    std::byte plain[PLAINTEXT_LENGTH];
    std::byte blob[ENCIPHERED_LENGTH];
    std::byte key[AEADChaCha20Poly1305::KEYLEN];
    std::byte cipher[PLAINTEXT_LENGTH + AEADChaCha20Poly1305::EXPANSION];

    plain[0] = std::byte{0}; // inner version
    plain[1] = std::byte(birthday_days & 0xFF);
    plain[2] = std::byte(birthday_days >> 8);
    std::copy(entropy.begin(), entropy.end(), plain + 3);

    blob[0] = std::byte{0}; // outer version
    std::copy(salt.begin(), salt.end(), blob + 1);

    DeriveKey(password, salt, key);

    // Zero nonce: the key is derived from a fresh salt and used exactly once.
    // AAD covers the outer version and salt so the tag authenticates them too.
    AEADChaCha20Poly1305 aead{key};
    aead.Encrypt(plain, Span{blob, 1 + SALT_LENGTH}, {0, 0}, cipher);

    // The ciphertext followed by the leading TAG_LENGTH bytes of the tag.
    std::copy(cipher, cipher + PLAINTEXT_LENGTH + TAG_LENGTH, blob + 1 + SALT_LENGTH);

    SecureString seed_phrase = EncodeSeedPhrase(blob);

    memory_cleanse(plain, sizeof(plain));
    memory_cleanse(blob, sizeof(blob));
    memory_cleanse(key, sizeof(key));
    memory_cleanse(cipher, sizeof(cipher));

    return seed_phrase;
}

SecureString GRC::Mnemonics::GenerateSeedPhrase(const SecureString& password, CKey& key_out)
{
    std::byte entropy[ENTROPY_LENGTH];
    std::byte salt[SALT_LENGTH];

    do {
        GetStrongRandBytes({UCharCast(entropy), sizeof(entropy)});
    } while (!DeriveWalletKey(entropy, key_out));

    GetStrongRandBytes({UCharCast(salt), sizeof(salt)});

    int64_t days = (GetTime() - BIRTHDAY_EPOCH) / (24 * 60 * 60);
    days = std::max<int64_t>(0, std::min<int64_t>(days, 0xFFFF));

    SecureString seed_phrase = BuildSeedPhrase(entropy, (uint16_t)days, salt, password);

    memory_cleanse(entropy, sizeof(entropy));
    memory_cleanse(salt, sizeof(salt));

    return seed_phrase;
}

bool GRC::Mnemonics::ParseSeedPhrase(const SecureString& seed_phrase, const SecureString& password,
                                     CKey& key_out, int64_t* birthday_out)
{
    std::byte blob[ENCIPHERED_LENGTH];
    std::byte key[AEADChaCha20Poly1305::KEYLEN];
    std::byte plain[PLAINTEXT_LENGTH];
    std::byte cipher[PLAINTEXT_LENGTH + AEADChaCha20Poly1305::EXPANSION];

    bool ok = false;
    int64_t birthday = 0;

    if (!DecodeSeedPhrase(seed_phrase, blob)) {
        goto cleanup;
    }

    // Outer version selects the decoding format; only version 0 exists.
    if (blob[0] != std::byte{0}) {
        goto cleanup;
    }

    DeriveKey(password, Span{blob + 1, SALT_LENGTH}, key);

    {
        // Decrypt: the payload keystream starts at block 1 (RFC 8439 reserves
        // block 0 for the Poly1305 key).
        ChaCha20 chacha{key};
        chacha.Seek({0, 0}, 1);
        chacha.Crypt(Span{blob + 1 + SALT_LENGTH, PLAINTEXT_LENGTH}, plain);

        // Verify the truncated tag: re-encrypt the recovered plaintext (the
        // ciphertext necessarily matches -- same key, nonce and keystream) and
        // compare the leading TAG_LENGTH bytes of the full tag, constant time.
        AEADChaCha20Poly1305 aead{key};
        aead.Encrypt(plain, Span{blob, 1 + SALT_LENGTH}, {0, 0}, cipher);

        if (timingsafe_bcmp(cipher + PLAINTEXT_LENGTH,
                            blob + 1 + SALT_LENGTH + PLAINTEXT_LENGTH, TAG_LENGTH) != 0) {
            goto cleanup;
        }
    }

    // Inner version selects the key derivation; only version 0 exists.
    if (plain[0] != std::byte{0}) {
        goto cleanup;
    }

    birthday = BIRTHDAY_EPOCH
        + ((int64_t)std::to_integer<unsigned>(plain[1])
           | ((int64_t)std::to_integer<unsigned>(plain[2]) << 8)) * 24 * 60 * 60;

    ok = DeriveWalletKey(Span{plain + 3, ENTROPY_LENGTH}, key_out);

    if (ok && birthday_out != nullptr) {
        *birthday_out = birthday;
    }

cleanup:
    memory_cleanse(blob, sizeof(blob));
    memory_cleanse(key, sizeof(key));
    memory_cleanse(plain, sizeof(plain));
    memory_cleanse(cipher, sizeof(cipher));

    return ok;
}
