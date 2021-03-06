// Author: Zhongwen(Alan) Lan(runbus@qq.com)
// Created: 2018/07/02
#include "gtest/gtest.h"
#include "etcd/client.h"
#include <chrono>
#include <thread>

namespace etcd {
class ClientTest : public testing::Test {
  protected:
    ClientTest() {
    }

    void SetUp() {
      // Fake mock: it requires to have etcd install first in local machine
      client_.reset(new Client("127.0.0.1:2379"));
    }
    void TearDown() {
    }
    std::unique_ptr<Client> client_;
};

TEST_F(ClientTest, Set) {
  const std::string key("/lzw/test/set");
  const std::string value("I am good");
  // Case Set once:
  bool ret = client_->Set(key, value, 0);
  EXPECT_TRUE(ret);
  std::string get_value = client_->Get(key);
  EXPECT_EQ(get_value, value);
  std::cout << "get_value: " << get_value << std::endl;
  // Case Set again:
  ret = client_->Set(key, value, 0);
  EXPECT_TRUE(ret);
  get_value = client_->Get(key);
  EXPECT_EQ(get_value, value);
}

TEST_F(ClientTest, Get) {
  // Case non-existing key:
  const std::string key("non-existing");
  std::string get_value = client_->Get(key);
  EXPECT_EQ(0, get_value.length());
  // Case existing key:
  const std::string exist_key("/lzw/testing/get");
  const std::string exist_value("jd.com");
  bool ret = client_->Set(exist_key, exist_value, 0);
  EXPECT_TRUE(ret);
  get_value = client_->Get(exist_key);
  EXPECT_EQ(exist_value, get_value);
  // Case update value
  const std::string updated_value("lzw.com");
  ret = client_->Set(exist_key, updated_value, 0);
  EXPECT_TRUE(ret);
  get_value = client_->Get(exist_key);
  EXPECT_EQ(updated_value, get_value);
}

TEST_F(ClientTest, Delete) {
  // Case delete non-existing key
  const std::string key("/non-existing");
  bool ret = client_->Delete(key);
  EXPECT_TRUE(ret);
  // Case delete existing key
  const std::string exist_key("/lzw/testing/get");
  const std::string exist_value("jd.com");
  ret = client_->Set(exist_key, exist_value, 0);
  EXPECT_TRUE(ret); 
  ret = client_->Delete(exist_key);
  EXPECT_TRUE(ret);
  // to check 
  std::string get_value = client_->Get(exist_key);
  EXPECT_EQ(0, get_value.length());
}

TEST_F(ClientTest, LeaseGrant) {
  int64_t ttl = 3;
  int64_t lease_id = client_->LeaseGrant(ttl);
  // Case check after ttl seconds
  const std::string exist_key("/lzw/testing/get");
  const std::string exist_value("jd.com");
  bool ret = client_->Set(exist_key, exist_value, lease_id);
  EXPECT_TRUE(ret); 
  std::string get_value = client_->Get(exist_key);
  EXPECT_EQ(exist_value, get_value);
  std::this_thread::sleep_for(std::chrono::seconds(ttl+1));
  get_value = client_->Get(exist_key);
  EXPECT_EQ(0, get_value.length());
}

TEST_F(ClientTest, KeepAlive) {
  int64_t ttl = 3;
  int64_t lease_id = client_->LeaseGrant(ttl);
  const std::string exist_key("/lzw/testing/get");
  const std::string exist_value("lzw.com");
  bool ret = client_->Set(exist_key, exist_value, lease_id);
  EXPECT_TRUE(ret); 
  std::string get_value = client_->Get(exist_key);
  EXPECT_EQ(exist_value, get_value);
  client_->KeepAlive(lease_id);
  std::this_thread::sleep_for(std::chrono::seconds(ttl+2));
  // Check value after ttl elapsed
  get_value = client_->Get(exist_key);
  EXPECT_TRUE(get_value.length() > 0);
  EXPECT_EQ(exist_value, get_value);
}

TEST_F(ClientTest, WatchGuard1) {
  // Case without keepalive but within ttl
  int64_t ttl = 5;
  int64_t lease_id = client_->LeaseGrant(ttl);
  const std::string exist_key("/lzw/testing/watch1");
  const std::string exist_value("lzw.com");
  bool ret = client_->Set(exist_key, exist_value, lease_id);
  EXPECT_TRUE(ret); 
  std::string get_value = client_->Get(exist_key);
  EXPECT_EQ(exist_value, get_value);
  client_->WatchGuard(exist_key, exist_value, ttl);
  std::this_thread::sleep_for(std::chrono::milliseconds(50));
  ret = client_->Delete(exist_key);
  EXPECT_TRUE(ret); 
  std::this_thread::sleep_for(std::chrono::milliseconds(500));
  get_value = client_->Get(exist_key);
  EXPECT_TRUE(get_value.length() > 0);
  EXPECT_EQ(exist_value, get_value);
  // Case without keepalive but out of ttl
  std::this_thread::sleep_for(std::chrono::seconds(ttl+2));
  get_value = client_->Get(exist_key);
  EXPECT_EQ(exist_value, get_value);
}

TEST_F(ClientTest, WatchGuard2) {
  int64_t ttl = 5;
  int64_t lease_id = client_->LeaseGrant(ttl);
  const std::string exist_key("/lzw/testing/watch2");
  const std::string exist_value("jd.com");
  // Case with keepalive
  lease_id = client_->LeaseGrant(ttl);
  bool ret = client_->Set(exist_key, exist_value, lease_id);
  EXPECT_TRUE(ret); 
  client_->KeepAlive(lease_id);
  client_->WatchGuard(exist_key, exist_value, ttl);
  // Can't Delete immediately after calling WatchGuard as it may NOT complete operations yet
  std::this_thread::sleep_for(std::chrono::milliseconds(50));
  ret = client_->Delete(exist_key);
  EXPECT_TRUE(ret); 
  std::this_thread::sleep_for(std::chrono::milliseconds(100));
  std::string get_value = client_->Get(exist_key);
  EXPECT_EQ(exist_value, get_value);
  std::this_thread::sleep_for(std::chrono::seconds(ttl+2));
  get_value = client_->Get(exist_key);
  EXPECT_EQ(exist_value, get_value);
  std::cout << "Existing WatchGuard2..." << std::endl;
}

TEST_F(ClientTest, Register) {
  int64_t ttl = 5;
  const std::string key("/lzw/testing/register");
  const std::string value("my.jd.com");
  bool ret = client_->Register(key, value, ttl);
  std::this_thread::sleep_for(std::chrono::milliseconds(100));
  EXPECT_TRUE(ret);
  ret = client_->Delete(key);
  EXPECT_TRUE(ret);
  std::this_thread::sleep_for(std::chrono::milliseconds(100));
  std::string get_value = client_->Get(key);
  EXPECT_EQ(value, get_value);
  std::this_thread::sleep_for(std::chrono::seconds(2*ttl));
  get_value = client_->Get(key);
  EXPECT_EQ(value, get_value);
}

} // namespace etcd
