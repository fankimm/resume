/**
 * MHXX SAVEDATA EDITOR FOR MAC
 * Reverse-engineered from original binary (2017)
 * Original author: crakim86@gmail.com
 *
 * Monster Hunter XX save data editor.
 * Reads a system save file and an item CSV, then allows
 * the user to modify item slots by changing item ID and quantity.
 *
 * Save file structure (item box region):
 *   - Data is BIT-PACKED, NOT byte-aligned
 *   - Each item slot = 19 bits:
 *       [0..11]  item ID   (12 bits, max 4096 — ~2400 items exist)
 *       [12..18] quantity   (7 bits, max 128 — game cap is 99)
 *   - Slots are stored consecutively in a bit stream
 *   - Slot N starts at bit offset (N-1) * 19
 *
 * This was the hardest part to figure out — a full week of hex diffing
 * before realizing the data wasn't byte-aligned but bit-packed.
 */

#include <iostream>
#include <fstream>
#include <sstream>
#include <string>
#include <vector>
#include <cstdint>

static const int ITEM_ID_BITS = 12;
static const int QUANTITY_BITS = 7;
static const int SLOT_BITS = ITEM_ID_BITS + QUANTITY_BITS; // 19 bits per slot

struct Item {
    int id;
    std::string name;
    std::string memo;
};

struct SaveSlot {
    uint16_t itemId;   // 12 bits (0..4095)
    uint8_t  quantity;  // 7 bits  (0..127)
};

// ── Bit-level read/write ──────────────────────────────────
// Read `numBits` from a byte array starting at bit offset `bitOffset`
uint32_t readBits(const std::vector<uint8_t>& data, size_t bitOffset, int numBits) {
    uint32_t result = 0;
    for (int i = 0; i < numBits; i++) {
        size_t byteIdx = (bitOffset + i) / 8;
        int bitIdx = (bitOffset + i) % 8;
        if (byteIdx < data.size()) {
            uint8_t bit = (data[byteIdx] >> bitIdx) & 1;
            result |= (bit << i);
        }
    }
    return result;
}

// Write `numBits` to a byte array starting at bit offset `bitOffset`
void writeBits(std::vector<uint8_t>& data, size_t bitOffset, int numBits, uint32_t value) {
    for (int i = 0; i < numBits; i++) {
        size_t byteIdx = (bitOffset + i) / 8;
        int bitIdx = (bitOffset + i) % 8;
        if (byteIdx < data.size()) {
            uint8_t bit = (value >> i) & 1;
            data[byteIdx] = (data[byteIdx] & ~(1 << bitIdx)) | (bit << bitIdx);
        }
    }
}

// ── Slot read/write using bit offsets ─────────────────────
SaveSlot readSlot(const std::vector<uint8_t>& data, int slotNumber) {
    size_t bitOffset = (slotNumber - 1) * SLOT_BITS;
    SaveSlot slot;
    slot.itemId  = static_cast<uint16_t>(readBits(data, bitOffset, ITEM_ID_BITS));
    slot.quantity = static_cast<uint8_t>(readBits(data, bitOffset + ITEM_ID_BITS, QUANTITY_BITS));
    return slot;
}

void writeSlot(std::vector<uint8_t>& data, int slotNumber, const SaveSlot& slot) {
    size_t bitOffset = (slotNumber - 1) * SLOT_BITS;
    writeBits(data, bitOffset, ITEM_ID_BITS, slot.itemId);
    writeBits(data, bitOffset + ITEM_ID_BITS, QUANTITY_BITS, slot.quantity);
}

// ── File I/O ──────────────────────────────────────────────
std::vector<Item> loadCsv(const std::string& path) {
    std::vector<Item> items;
    std::ifstream file(path);
    if (!file.is_open()) {
        std::cout << "csv file open error" << std::endl;
        return items;
    }

    std::string line;
    std::getline(file, line); // skip header: indexNumber,itemName,memo

    while (std::getline(file, line)) {
        if (line.empty()) continue;
        std::stringstream ss(line);
        std::string idStr, name, memo;

        std::getline(ss, idStr, ',');
        std::getline(ss, name, ',');
        std::getline(ss, memo, ',');

        Item item;
        item.id = std::stoi(idStr);
        item.name = name;
        item.memo = memo;
        items.push_back(item);
    }
    return items;
}

std::vector<uint8_t> loadSaveFile(const std::string& path) {
    std::ifstream file(path, std::ios::binary);
    if (!file.is_open()) {
        std::cout << "system file open error" << std::endl;
        return {};
    }
    return std::vector<uint8_t>(
        std::istreambuf_iterator<char>(file),
        std::istreambuf_iterator<char>()
    );
}

bool writeSaveFile(const std::string& path, const std::vector<uint8_t>& data) {
    std::ofstream file(path, std::ios::binary);
    if (!file.is_open()) return false;
    file.write(reinterpret_cast<const char*>(data.data()), data.size());
    return file.good();
}

// ── Helpers ───────────────────────────────────────────────
std::string findItemName(const std::vector<Item>& items, int id) {
    for (const auto& item : items) {
        if (item.id == id) return item.name;
    }
    return "Unknown";
}

void printSlotInfo(const SaveSlot& slot, const std::vector<Item>& items) {
    std::cout << "  ITEM ID = " << slot.itemId
              << " (" << findItemName(items, slot.itemId) << ")" << std::endl;
    std::cout << "  NUMBER = " << static_cast<int>(slot.quantity) << std::endl;
}

// ── Main ──────────────────────────────────────────────────
int main() {
    std::cout << std::endl;
    std::cout << "*********************************" << std::endl;
    std::cout << "MHXX SAVEDATA EDITOR FOR MAC 1.2" << std::endl;
    std::cout << "CRAKIM86@GMAIL.COM" << std::endl;
    std::cout << "*********************************" << std::endl;
    std::cout << std::endl;

    // Load save file
    std::string savePath;
    std::cout << "DRAG SYSTEM FILE HERE : ";
    std::getline(std::cin, savePath);
    if (!savePath.empty() && savePath.front() == '\'') savePath.erase(0, 1);
    if (!savePath.empty() && savePath.back() == '\'') savePath.pop_back();
    while (!savePath.empty() && savePath.back() == ' ') savePath.pop_back();

    std::vector<uint8_t> saveData = loadSaveFile(savePath);
    if (saveData.empty()) return 1;

    // Load CSV
    std::string csvPath;
    std::cout << "DRAG CSV FILE HERE : ";
    std::getline(std::cin, csvPath);
    if (!csvPath.empty() && csvPath.front() == '\'') csvPath.erase(0, 1);
    if (!csvPath.empty() && csvPath.back() == '\'') csvPath.pop_back();
    while (!csvPath.empty() && csvPath.back() == ' ') csvPath.pop_back();

    std::vector<Item> items = loadCsv(csvPath);
    if (items.empty()) return 1;

    int maxSlots = static_cast<int>((saveData.size() * 8) / SLOT_BITS);

    // Main loop
    while (true) {
        std::cout << std::endl;
        std::cout << "-------------" << std::endl;
        std::cout << "1. ITEM EDIT" << std::endl;
        std::cout << "2. EXIT" << std::endl;
        std::cout << "-------------" << std::endl;

        int choice;
        std::cin >> choice;

        if (choice == 2) break;
        if (choice != 1) continue;

        std::cout << "------------------------------------" << std::endl;
        std::cout << "ENTER ITEM SLOT NUMBER TO CHANGE : ";
        int slotNum;
        std::cin >> slotNum;

        if (slotNum < 1 || slotNum > maxSlots) {
            std::cout << "ENTER PROPER" << std::endl;
            continue;
        }

        // Show current slot info
        SaveSlot slot = readSlot(saveData, slotNum);
        std::cout << "----------------------" << std::endl;
        std::cout << "SELECTED ITEM INFO : " << std::endl;
        printSlotInfo(slot, items);

        // Get new item ID
        std::cout << "ENTER ITEM ID TO CHANGE : ";
        int newId;
        std::cin >> newId;

        // Get new quantity
        std::cout << "ENTER THE NUMBER OF ITEMS TO REPLACE : ";
        int newQty;
        std::cin >> newQty;

        // Apply changes
        slot.itemId = static_cast<uint16_t>(newId & 0xFFF);  // 12-bit mask
        slot.quantity = static_cast<uint8_t>(newQty & 0x7F);  // 7-bit mask
        writeSlot(saveData, slotNum, slot);

        // Show modified info
        std::cout << "--------------------------------" << std::endl;
        std::cout << "ITEM INFO AFTER MODIFICATION : " << std::endl;
        printSlotInfo(slot, items);
    }

    // Save modified file
    writeSaveFile(savePath, saveData);
    std::cout << "Save complete." << std::endl;

    return 0;
}
