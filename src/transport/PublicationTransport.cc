/*
 * Copyright 2011 Nate Koenig & Andrew Howard
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 *
*/

#include "transport/TopicManager.hh"
#include "transport/ConnectionManager.hh"
#include "transport/PublicationTransport.hh"

using namespace gazebo;
using namespace transport;

int PublicationTransport::counter = 0;

PublicationTransport::PublicationTransport(const std::string &topic,
                                           const std::string &msgType)
: topic(topic), msgType(msgType)
{
  this->id = counter++;
  TopicManager::Instance()->UpdatePublications(topic, msgType);
}

PublicationTransport::~PublicationTransport()
{
  if (this->connection)
  {
    this->connection->DisconnectShutdown(this->shutdownConnectionPtr);

    msgs::Subscribe sub;
    sub.set_topic(this->topic);
    sub.set_msg_type(this->msgType);
    sub.set_host(this->connection->GetLocalAddress());
    sub.set_port(this->connection->GetLocalPort());
    ConnectionManager::Instance()->Unsubscribe(sub);
    this->connection->Cancel();
    this->connection.reset();

    ConnectionManager::Instance()->RemoveConnection(this->connection);
  }
  this->callback.clear();
}

void PublicationTransport::Init(const ConnectionPtr &conn_)
{
  this->connection = conn_;
  msgs::Subscribe sub;
  sub.set_topic(this->topic);
  sub.set_msg_type(this->msgType);
  sub.set_host(this->connection->GetLocalAddress());
  sub.set_port(this->connection->GetLocalPort());

  this->connection->EnqueueMsg(msgs::Package("sub", sub));

  // Put this in PublicationTransportPtr
  // Start reading messages from the remote publisher
  this->connection->AsyncRead(boost::bind(&PublicationTransport::OnPublish,
        this, _1));

  this->shutdownConnectionPtr = this->connection->ConnectToShutdown(
      boost::bind(&PublicationTransport::OnConnectionShutdown, this));
}

void PublicationTransport::OnConnectionShutdown()
{
}

void PublicationTransport::AddCallback(
    const boost::function<void(const std::string &)> &cb_)
{
  this->callback = cb_;
}

void PublicationTransport::OnPublish(const std::string &_data)
{
  if (this->connection && this->connection->IsOpen())
  {
    this->connection->AsyncRead(
        boost::bind(&PublicationTransport::OnPublish, this, _1));

    if (!_data.empty())
    {
      if (this->callback)
        (this->callback)(_data);
    }
  }
}

const ConnectionPtr PublicationTransport::GetConnection() const
{
  return this->connection;
}

std::string PublicationTransport::GetTopic() const
{
  return this->topic;
}

std::string PublicationTransport::GetMsgType() const
{
  return this->msgType;
}

void PublicationTransport::Fini()
{
  /// Cancel all async operatiopns.
  this->connection->Cancel();
  this->connection.reset();
}



