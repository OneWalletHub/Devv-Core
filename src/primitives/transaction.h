/*
 * transaction.h
 *
 *  Created on: Dec 11, 2017
 *      Author: Nick Williams
 *
 *      **Transaction Structure**
 *      oper - coin operation (one of create, modify, exchange, delete)
 *		type - the coin type
 *		xfer - array of participants
 *
 *		xfer params:
 *		addr - wallet address
 *		amount - postive implies giving, negative implies taking, 0 implies neither
 *		nonce - generated by wallet (required iff amount positive)
 *		sig - generated by wallet (required iff amount positive)
 *
 *
 */

#ifndef DEVCASH_PRIMITIVES_TRANSACTION_H
#define DEVCASH_PRIMITIVES_TRANSACTION_H

#include <string>
#include <vector>
#include <stdint.h>
#include "../ossladapter.h"
#include "../json.hpp"
using json = nlohmann::json;

static const std::string TX_TAG = "txs";

enum OpType : char {
	Create     = 0x00,
	Modify     = 0x01,
	Exchange   = 0x10,
	Delete     = 0x11};

class DCTransfer {
public:
	std::string addr;
	long amount;
	uint32_t nonce;
	std::string sig;

    void SetNull() { addr = ""; amount = 0; }
    bool IsNull() const { return ("" == addr); }
    DCTransfer();
	explicit DCTransfer(std::string json);
	explicit DCTransfer(const char* cbor);
	explicit DCTransfer(const DCTransfer &other);
	std::string getCanonical();
    friend bool operator==(const DCTransfer& a, const DCTransfer& b)
    {
        return (a.addr == b.addr && a.amount == b.amount && a.nonce == b.nonce);
    }

    friend bool operator!=(const DCTransfer& a, const DCTransfer& b)
    {
    	return !(a.addr == b.addr && a.amount == b.amount && a.nonce == b.nonce);
    }
    DCTransfer* operator=(DCTransfer&& other)
	{
    	if (this != &other) {
    		this->addr = other.addr;
    		this->amount = other.amount;
    		this->nonce = other.nonce;
    		this->sig = other.sig;
    	}
    	return this;
    }
    DCTransfer* operator=(const DCTransfer& other)
	{
    	if (this != &other) {
    		this->addr = other.addr;
    		this->amount = other.amount;
    		this->nonce = other.nonce;
    		this->sig = other.sig;
    	}
    	return this;
    }
};

class DCTransaction {
public:
	OpType oper;
	std::string type;
	std::vector<DCTransfer>* xfers;
	DCTransaction();
	explicit DCTransaction(std::string jsonTx);
	explicit DCTransaction(json jsonObj);
	explicit DCTransaction(char* cbor);
	DCTransaction(const DCTransaction& tx);
	bool isValid(EC_KEY* eckey) const;
	std::string ComputeHash() const;

private:
    std::string hash = "";
    std::string jsonStr = "";
    std::string GetCanonical() const;

public:
    const std::string& GetHash() const {
        return hash;
    }

    // Return sum of xfers.
    long GetValueOut() const;
    unsigned int GetByteSize() const;
    friend bool operator==(const DCTransaction& a, const DCTransaction& b)
    {
        return a.hash == b.hash;
    }

    friend bool operator!=(const DCTransaction& a, const DCTransaction& b)
    {
        return a.hash != b.hash;
    }
    DCTransaction* operator=(DCTransaction&& other)
	{
    	std::cout << "&&TX " << std::endl;
    	if (this != &other) {
    		this->hash = other.hash;
    		this->jsonStr = other.jsonStr;
    		this->oper = other.oper;
    		this->type = other.type;
    		this->xfers = std::move(other.xfers);
    	}
    	return this;
    }
    DCTransaction* operator=(const DCTransaction& other)
	{
    	std::cout << "&TX " << std::endl;
    	if (this != &other) {
    		this->hash = other.hash;
    		this->jsonStr = other.jsonStr;
    		this->oper = other.oper;
    		this->type = other.type;
    		this->xfers = std::move(other.xfers);
    	}
    	return this;
    }

    std::string ToJSON() const;
    std::vector<uint8_t> ToCBOR() const;
};

#endif // DEVCASH_PRIMITIVES_TRANSACTION_H

