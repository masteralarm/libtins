#include <algorithm>
#include <gtest/gtest.h>
#include "ethernetII.h"
#include "utils.h"
#include "macros.h"
#include "ipv6.h"
#include "ip.h"

using namespace Tins;

typedef EthernetII::address_type address_type;

class EthernetIITest : public ::testing::Test {
public:
    static const uint8_t expected_packet[], ip_packet[], ipv6_packet[];
    static address_type src_addr;
    static address_type dst_addr;
    static address_type empty_addr;
    static const uint16_t p_type;

    void test_equals(const EthernetII &eth1, const EthernetII &eth2);
};

const uint8_t EthernetIITest::expected_packet[] = {
    170, 187, 204, 221, 238, 255, 138, 139, 140, 141, 142, 143, 208, 171
},
EthernetIITest::ip_packet[] = {
    255, 255, 255, 255, 255, 255, 0, 0, 0, 0, 0, 0, 8, 0, 69, 0, 0, 20, 
    0, 1, 0, 0, 64, 0, 124, 231, 127, 0, 0, 1, 127, 0, 0, 1
},
EthernetIITest::ipv6_packet[] = {
    255, 255, 255, 255, 255, 255, 0, 0, 0, 0, 0, 0, 134, 221, 96, 0, 0, 
    0, 0, 0, 59, 64, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 1, 0, 
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 1
};

address_type EthernetIITest::src_addr("8a:8b:8c:8d:8e:8f");

address_type EthernetIITest::dst_addr("aa:bb:cc:dd:ee:ff");

address_type EthernetIITest::empty_addr;

const uint16_t EthernetIITest::p_type = 0xd0ab;

void EthernetIITest::test_equals(const EthernetII &eth1, const EthernetII &eth2) {
    EXPECT_EQ(eth1.dst_addr(), eth2.dst_addr());
    EXPECT_EQ(eth1.src_addr(), eth2.src_addr());
    EXPECT_EQ(eth1.payload_type(), eth2.payload_type());
    EXPECT_EQ((bool)eth1.inner_pdu(), (bool)eth2.inner_pdu());
}

TEST_F(EthernetIITest, DefaultConstructor) {
    EthernetII eth;
    EXPECT_EQ(eth.dst_addr(), empty_addr);
    EXPECT_EQ(eth.src_addr(), empty_addr);
    EXPECT_EQ(eth.payload_type(), 0);
    EXPECT_FALSE(eth.inner_pdu());
    EXPECT_EQ(eth.pdu_type(), PDU::ETHERNET_II);
}

TEST_F(EthernetIITest, CopyConstructor) {
    EthernetII eth1(expected_packet, sizeof(expected_packet));
    EthernetII eth2(eth1);
    test_equals(eth1, eth2);
}

TEST_F(EthernetIITest, CopyAssignmentOperator) {
    EthernetII eth1(expected_packet, sizeof(expected_packet));
    EthernetII eth2;
    eth2 = eth1;
    test_equals(eth1, eth2);
}

TEST_F(EthernetIITest, NestedCopy) {
    EthernetII *nested = new EthernetII(expected_packet, sizeof(expected_packet));
    EthernetII eth1(expected_packet, sizeof(expected_packet));
    eth1.inner_pdu(nested);
    EthernetII eth2(eth1);
    test_equals(eth1, eth2);
}

TEST_F(EthernetIITest, SourceAddress) {
    EthernetII eth;
    eth.src_addr(src_addr);
    ASSERT_EQ(eth.src_addr(), src_addr);
}

TEST_F(EthernetIITest, DestinationAddress) {
    EthernetII eth;
    eth.dst_addr(dst_addr);
    ASSERT_EQ(eth.dst_addr(), dst_addr);
}

TEST_F(EthernetIITest, PayloadType) {
    EthernetII eth;
    eth.payload_type(p_type);
    ASSERT_EQ(eth.payload_type(), p_type);
}

TEST_F(EthernetIITest, CompleteConstructor) {
    EthernetII* eth2 = new EthernetII();
    EthernetII eth(dst_addr, src_addr, eth2);
    EXPECT_EQ(eth.dst_addr(), dst_addr);
    EXPECT_EQ(eth.src_addr(), src_addr);
    EXPECT_TRUE(eth.inner_pdu() == eth2);
    EXPECT_EQ(eth.payload_type(), 0);
}

TEST_F(EthernetIITest, Serialize) {
    EthernetII eth(dst_addr, src_addr);
    eth.payload_type(p_type);
    PDU::serialization_type serialized = eth.serialize();
    ASSERT_EQ(serialized.size(), sizeof(expected_packet));
    EXPECT_TRUE(std::equal(serialized.begin(), serialized.end(), expected_packet));
}

TEST_F(EthernetIITest, ConstructorFromBuffer) {
    EthernetII eth(expected_packet, sizeof(expected_packet));
    EXPECT_EQ(eth.src_addr(), src_addr);
    EXPECT_EQ(eth.dst_addr(), dst_addr);
    EXPECT_EQ(eth.payload_type(), p_type);
}

TEST_F(EthernetIITest, ConstructorFromIPBuffer) {
    EthernetII eth(ip_packet, sizeof(ip_packet));
    ASSERT_TRUE(eth.inner_pdu());
    EXPECT_EQ(eth.find_pdu<IP>(), eth.inner_pdu());
}

TEST_F(EthernetIITest, ConstructorFromIPv6Buffer) {
    EthernetII eth(ipv6_packet, sizeof(ipv6_packet));
    ASSERT_TRUE(eth.inner_pdu());
    EXPECT_EQ(eth.find_pdu<IPv6>(), eth.inner_pdu());
}
