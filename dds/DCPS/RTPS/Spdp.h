/*
 * Distributed under the OpenDDS License.
 * See: http://www.opendds.org/license.html
 */

#ifndef OPENDDS_DCPS_RTPS_SPDP_H
#define OPENDDS_DCPS_RTPS_SPDP_H

#include "Sedp.h"
#include "rtps_export.h"
#include "ICE/Ice.h"
#include "RtpsCoreC.h"

#include <dds/DCPS/RcObject.h>
#include <dds/DCPS/GuidUtils.h>
#include <dds/DCPS/Definitions.h>
#include <dds/DCPS/RcEventHandler.h>
#include <dds/DCPS/ReactorTask.h>
#include <dds/DCPS/PeriodicTask.h>
#include <dds/DCPS/SporadicTask.h>
#include <dds/DCPS/MultiTask.h>
#include <dds/DCPS/JobQueue.h>
#include <dds/DCPS/NetworkConfigMonitor.h>
#include <dds/DCPS/FibonacciSequence.h>
#include <dds/DCPS/BuiltInTopicDataReaderImpls.h>
#include <dds/DCPS/security/framework/SecurityConfig_rch.h>
#ifdef OPENDDS_SECURITY
#  include <dds/DCPS/security/framework/SecurityConfig.h>
#endif
#include <dds/DCPS/PoolAllocator.h>
#include <dds/DCPS/PoolAllocationBase.h>
#include <dds/DCPS/TimeTypes.h>

#include <dds/DdsDcpsInfrastructureC.h>
#include <dds/DdsDcpsInfoUtilsC.h>
#include <dds/DdsDcpsCoreTypeSupportImpl.h>

#ifndef ACE_HAS_CPP11
#  include <ace/Atomic_Op.h>
#endif
#include <ace/SOCK_Dgram.h>
#include <ace/SOCK_Dgram_Mcast.h>
#include <ace/Thread_Mutex.h>

#ifdef ACE_HAS_CPP11
#  include <atomic>
#endif

#ifndef ACE_LACKS_PRAGMA_ONCE
#  pragma once
#endif

/* ParticipantData_t */

OPENDDS_BEGIN_VERSIONED_NAMESPACE_DECL

namespace OpenDDS {
namespace RTPS {

class RtpsDiscoveryConfig;
class RtpsDiscovery;

const char SPDP_AGENT_INFO_KEY[] = "SPDP";
const char SEDP_AGENT_INFO_KEY[] = "SEDP";

/// Each instance of class Spdp represents the implementation of the RTPS
/// Simple Participant Discovery Protocol for a single local DomainParticipant.
class OpenDDS_Rtps_Export Spdp
  : public DCPS::RcObject
#ifdef OPENDDS_SECURITY
  , public ICE::AgentInfoListener
#endif
{
public:
  struct DiscoveredParticipant {

    DiscoveredParticipant()
    : location_ih_(DDS::HANDLE_NIL)
    , bit_ih_(DDS::HANDLE_NIL)
    , seq_reset_count_(0)
#ifdef OPENDDS_SECURITY
    , have_spdp_info_(false)
    , have_sedp_info_(false)
    , have_auth_req_msg_(false)
    , have_handshake_msg_(false)
    , handshake_resend_falloff_(TimeDuration::zero_value)
    , auth_state_(AUTH_STATE_HANDSHAKE)
    , handshake_state_(HANDSHAKE_STATE_BEGIN_HANDSHAKE_REQUEST)
    , is_requester_(false)
    , auth_req_sequence_number_(0)
    , handshake_sequence_number_(0)
    , identity_handle_(DDS::HANDLE_NIL)
    , handshake_handle_(DDS::HANDLE_NIL)
    , permissions_handle_(DDS::HANDLE_NIL)
    , extended_builtin_endpoints_(0)
#endif
    {
#ifdef OPENDDS_SECURITY
      security_info_.participant_security_attributes = 0;
      security_info_.plugin_participant_security_attributes = 0;
#endif
    }

    DiscoveredParticipant(
      const ParticipantData_t& p,
      const SequenceNumber& seq,
      const TimeDuration& resend_period)
    : pdata_(p)
    , location_ih_(DDS::HANDLE_NIL)
    , bit_ih_(DDS::HANDLE_NIL)
    , last_seq_(seq)
    , seq_reset_count_(0)
#ifdef OPENDDS_SECURITY
    , have_spdp_info_(false)
    , have_sedp_info_(false)
    , have_auth_req_msg_(false)
    , have_handshake_msg_(false)
    , handshake_resend_falloff_(resend_period)
    , auth_state_(AUTH_STATE_HANDSHAKE)
    , handshake_state_(HANDSHAKE_STATE_BEGIN_HANDSHAKE_REQUEST)
    , is_requester_(false)
    , auth_req_sequence_number_(0)
    , handshake_sequence_number_(0)
    , identity_handle_(DDS::HANDLE_NIL)
    , handshake_handle_(DDS::HANDLE_NIL)
    , permissions_handle_(DDS::HANDLE_NIL)
    , extended_builtin_endpoints_(0)
#endif
    {
      const GUID_t guid = DCPS::make_part_guid(p.participantProxy.guidPrefix);
      assign(location_data_.guid, guid);
      location_data_.location = 0;
      location_data_.change_mask = 0;
      location_data_.local_timestamp.sec = 0;
      location_data_.local_timestamp.nanosec = 0;
      location_data_.ice_timestamp.sec = 0;
      location_data_.ice_timestamp.nanosec = 0;
      location_data_.relay_timestamp.sec = 0;
      location_data_.relay_timestamp.nanosec = 0;
      location_data_.local6_timestamp.sec = 0;
      location_data_.local6_timestamp.nanosec = 0;
      location_data_.ice6_timestamp.sec = 0;
      location_data_.ice6_timestamp.nanosec = 0;
      location_data_.relay6_timestamp.sec = 0;
      location_data_.relay6_timestamp.nanosec = 0;

#ifdef OPENDDS_SECURITY
      security_info_.participant_security_attributes = 0;
      security_info_.plugin_participant_security_attributes = 0;
#else
      ACE_UNUSED_ARG(resend_period);
#endif
    }

    ParticipantData_t pdata_;

    struct LocationUpdate {
      DCPS::ParticipantLocation mask_;
      ACE_INET_Addr from_;
      SystemTimePoint timestamp_;
      LocationUpdate() {}
      LocationUpdate(DCPS::ParticipantLocation mask,
                     const ACE_INET_Addr& from,
                     const SystemTimePoint& timestamp)
        : mask_(mask), from_(from), timestamp_(timestamp) {}
    };
    typedef OPENDDS_VECTOR(LocationUpdate) LocationUpdateList;
    LocationUpdateList location_updates_;
    DCPS::ParticipantLocationBuiltinTopicData location_data_;
    DDS::InstanceHandle_t location_ih_;

    ACE_INET_Addr local_address_;
    MonotonicTimePoint discovered_at_;
    MonotonicTimePoint lease_expiration_;
    DDS::InstanceHandle_t bit_ih_;
    SequenceNumber last_seq_;
    ACE_UINT16 seq_reset_count_;
#ifdef OPENDDS_SECURITY
    bool have_spdp_info_;
    ICE::AgentInfo spdp_info_;
    bool have_sedp_info_;
    ICE::AgentInfo sedp_info_;
    bool have_auth_req_msg_;
    DDS::Security::ParticipantStatelessMessage auth_req_msg_;
    bool have_handshake_msg_;
    DDS::Security::ParticipantStatelessMessage handshake_msg_;
    DCPS::FibonacciSequence<TimeDuration> handshake_resend_falloff_;
    MonotonicTimePoint stateless_msg_deadline_;

    MonotonicTimePoint handshake_deadline_;
    AuthState auth_state_;
    HandshakeState handshake_state_;
    bool is_requester_;
    CORBA::LongLong auth_req_sequence_number_;
    CORBA::LongLong handshake_sequence_number_;

    DDS::Security::IdentityToken identity_token_;
    DDS::Security::PermissionsToken permissions_token_;
    DDS::Security::PropertyQosPolicy property_qos_;
    DDS::Security::ParticipantSecurityInfo security_info_;
    DDS::Security::IdentityStatusToken identity_status_token_;
    DDS::Security::IdentityHandle identity_handle_;
    DDS::Security::HandshakeHandle handshake_handle_;
    DDS::Security::AuthRequestMessageToken local_auth_request_token_;
    DDS::Security::AuthRequestMessageToken remote_auth_request_token_;
    DDS::Security::AuthenticatedPeerCredentialToken authenticated_peer_credential_token_;
    DDS::Security::SharedSecretHandle_var shared_secret_handle_;
    DDS::Security::PermissionsHandle permissions_handle_;
    DDS::Security::ParticipantCryptoTokenSeq crypto_tokens_;
    DDS::Security::ExtendedBuiltinEndpointSet_t extended_builtin_endpoints_;
#endif
  };

  typedef OPENDDS_MAP_CMP(GUID_t, DiscoveredParticipant,
                          GUID_tKeyLessThan) DiscoveredParticipantMap;
  typedef DiscoveredParticipantMap::iterator DiscoveredParticipantIter;
  typedef DiscoveredParticipantMap::const_iterator DiscoveredParticipantConstIter;


  Spdp(DDS::DomainId_t domain,
       DCPS::RepoId& guid,
       const DDS::DomainParticipantQos& qos,
       RtpsDiscovery* disco,
       XTypes::TypeLookupService_rch tls);

#ifdef OPENDDS_SECURITY
  Spdp(DDS::DomainId_t domain,
       const DCPS::RepoId& guid,
       const DDS::DomainParticipantQos& qos,
       RtpsDiscovery* disco,
       XTypes::TypeLookupService_rch tls,
       DDS::Security::IdentityHandle id_handle,
       DDS::Security::PermissionsHandle perm_handle,
       DDS::Security::ParticipantCryptoHandle crypto_handle);
#endif

  ~Spdp();

  // Participant
  const DCPS::RepoId& guid() const { return guid_; }
  void init_bit(const DDS::Subscriber_var& bit_subscriber);
  void fini_bit();

  bool get_default_locators(const DCPS::RepoId& part_id,
                            DCPS::LocatorSeq& target,
                            bool& inlineQos);

  // Managing reader/writer associations
  void signal_liveliness(DDS::LivelinessQosPolicyKind kind);

  // Is Spdp fully initialized?
  bool initialized()
  {
#ifdef ACE_HAS_CPP11
    return initialized_flag_;
#else
    return initialized_flag_.value();
#endif
  }

  void shutdown();

  // Is Spdp shutting down?
  bool shutting_down()
  {
#ifdef ACE_HAS_CPP11
    return shutdown_flag_;
#else
    return shutdown_flag_.value();
#endif
  }

  bool associated() const;
  bool has_discovered_participant(const DCPS::RepoId& guid) const;
  ACE_CDR::ULong get_participant_flags(const DCPS::RepoId& guid) const;

#ifdef OPENDDS_SECURITY
  Security::SecurityConfig_rch get_security_config() const { return security_config_; }
  DDS::Security::ParticipantCryptoHandle crypto_handle() const { return crypto_handle_; }
  DDS::Security::ParticipantCryptoHandle remote_crypto_handle(const DCPS::RepoId& remote_participant) const;

  void handle_auth_request(const DDS::Security::ParticipantStatelessMessage& msg);
  void send_handshake_request(const DCPS::RepoId& guid, DiscoveredParticipant& dp);
  void handle_handshake_message(const DDS::Security::ParticipantStatelessMessage& msg);
  bool handle_participant_crypto_tokens(const DDS::Security::ParticipantVolatileMessageSecure& msg);
  DDS::OctetSeq local_participant_data_as_octets() const;
#endif

  void handle_participant_data(DCPS::MessageId id,
                               const ParticipantData_t& pdata,
                               const DCPS::SequenceNumber& seq,
                               const ACE_INET_Addr& from,
                               bool from_sedp);

  bool validateSequenceNumber(const DCPS::MonotonicTimePoint& now, const DCPS::SequenceNumber& seq, DiscoveredParticipantIter& iter);

#ifdef OPENDDS_SECURITY
  void process_handshake_deadlines(const DCPS::MonotonicTimePoint& tv);
  void process_handshake_resends(const DCPS::MonotonicTimePoint& tv);

  /**
   * Write Secured Updated DP QOS
   *
   * lock_ must be acquired before calling this.
   */
  void write_secure_updates();
  void write_secure_disposes();
  bool is_security_enabled() const { return security_enabled_; }
#endif

  bool is_expectant_opendds(const GUID_t& participant) const;

#ifdef OPENDDS_SECURITY
  typedef std::pair<DDS::Security::ParticipantCryptoHandle, DDS::Security::SharedSecretHandle_var> ParticipantCryptoInfoPair;
  ParticipantCryptoInfoPair lookup_participant_crypto_info(const DCPS::RepoId& id) const;
  void send_participant_crypto_tokens(const DCPS::RepoId& id);

  DDS::DomainId_t get_domain_id() const { return domain_; }
  DDS::Security::PermissionsHandle lookup_participant_permissions(const DCPS::RepoId& id) const;

  AuthState lookup_participant_auth_state(const GUID_t& id) const;

  void process_participant_ice(const ParameterList& plist,
                               const ParticipantData_t& pdata,
                               const DCPS::RepoId& guid);

#endif

  const ParticipantData_t& get_participant_data(const DCPS::RepoId& guid) const;
  ParticipantData_t& get_participant_data(const DCPS::RepoId& guid);
  DCPS::MonotonicTime_t get_participant_discovered_at() const;
  DCPS::MonotonicTime_t get_participant_discovered_at(const DCPS::RepoId& guid) const;

  u_short get_spdp_port() const { return tport_ ? tport_->uni_port_ : 0; }

  u_short get_sedp_port() const { return sedp_->local_address().get_port_number(); }

#ifdef ACE_HAS_IPV6
  u_short get_ipv6_spdp_port() const { return tport_ ? tport_->ipv6_uni_port_ : 0; }

  u_short get_ipv6_sedp_port() const { return sedp_->ipv6_local_address().get_port_number(); }
#endif

  void rtps_relay_only_now(bool f);
  void use_rtps_relay_now(bool f);
  void use_ice_now(bool f);
  void sedp_rtps_relay_address(const ACE_INET_Addr& address) { sedp_->rtps_relay_address(address); }
  void sedp_stun_server_address(const ACE_INET_Addr& address) { sedp_->stun_server_address(address); }

  BuiltinEndpointSet_t available_builtin_endpoints() const { return available_builtin_endpoints_; }
#ifdef OPENDDS_SECURITY
  DDS::Security::ExtendedBuiltinEndpointSet_t available_extended_builtin_endpoints() const
  {
    return available_extended_builtin_endpoints_;
  }
#endif

  ICE::Endpoint* get_ice_endpoint_if_added();

  ParticipantData_t build_local_pdata(
#ifdef OPENDDS_SECURITY
    bool always_in_the_clear,
    Security::DiscoveredParticipantDataKind kind
#endif
  );

  DCPS::RcHandle<RtpsDiscoveryConfig> config() const { return config_; }
  void send_to_relay();

  void get_and_reset_relay_message_counts(DCPS::RelayMessageCounts& spdp,
                                          DCPS::RelayMessageCounts& sedp);

  void ignore_domain_participant(const GUID_t& ignoreId);

  bool update_domain_participant_qos(const DDS::DomainParticipantQos& qos);

  bool has_domain_participant(const GUID_t& ignoreId) const;

  DCPS::TopicStatus assert_topic(GUID_t& topicId, const char* topicName,
    const char* dataTypeName, const DDS::TopicQos& qos,
    bool hasDcpsKey, DCPS::TopicCallbacks* topic_callbacks);

  DCPS::TopicStatus find_topic(
    const char* topicName,
    CORBA::String_out dataTypeName,
    DDS::TopicQos_out qos,
    GUID_t& topicId)
  {
    return endpoint_manager().find_topic(topicName, dataTypeName, qos, topicId);
  }

  DCPS::TopicStatus remove_topic(const GUID_t& topicId)
  {
    return endpoint_manager().remove_topic(topicId);
  }

  void ignore_topic(const GUID_t& ignoreId)
  {
    ACE_GUARD(ACE_Thread_Mutex, g, lock_);
    endpoint_manager().ignore(ignoreId);
  }

  bool update_topic_qos(const GUID_t& topicId, const DDS::TopicQos& qos)
  {
    return endpoint_manager().update_topic_qos(topicId, qos);
  }

  GUID_t add_publication(
    const GUID_t& topicId,
    DCPS::DataWriterCallbacks_rch publication,
    const DDS::DataWriterQos& qos,
    const DCPS::TransportLocatorSeq& transInfo,
    const DDS::PublisherQos& publisherQos,
    const XTypes::TypeInformation& type_info)
  {
    return endpoint_manager().add_publication(topicId, publication, qos,
                                              transInfo, publisherQos, type_info);
  }

  void remove_publication(const GUID_t& publicationId)
  {
    endpoint_manager().remove_publication(publicationId);
  }

  void ignore_publication(const GUID_t& ignoreId)
  {
    ACE_GUARD(ACE_Thread_Mutex, g, lock_);
    return endpoint_manager().ignore(ignoreId);
  }

  bool update_publication_qos(
    const GUID_t& publicationId,
    const DDS::DataWriterQos& qos,
    const DDS::PublisherQos& publisherQos)
  {
    return endpoint_manager().update_publication_qos(publicationId, qos, publisherQos);
  }

  void update_publication_locators(const GUID_t& publicationId,
                                   const DCPS::TransportLocatorSeq& transInfo)
  {
    endpoint_manager().update_publication_locators(publicationId, transInfo);
  }

  GUID_t add_subscription(
    const GUID_t& topicId,
    DCPS::DataReaderCallbacks_rch subscription,
    const DDS::DataReaderQos& qos,
    const DCPS::TransportLocatorSeq& transInfo,
    const DDS::SubscriberQos& subscriberQos,
    const char* filterClassName,
    const char* filterExpr,
    const DDS::StringSeq& params,
    const XTypes::TypeInformation& type_info)
  {
    return endpoint_manager().add_subscription(topicId, subscription, qos, transInfo,
      subscriberQos, filterClassName, filterExpr, params, type_info);
  }

  void remove_subscription(const GUID_t& subscriptionId)
  {
    endpoint_manager().remove_subscription(subscriptionId);
  }

  void ignore_subscription(const GUID_t& ignoreId)
  {
    ACE_GUARD(ACE_Thread_Mutex, g, lock_);
    return endpoint_manager().ignore(ignoreId);
  }

  bool update_subscription_qos(
    const GUID_t& subscriptionId,
    const DDS::DataReaderQos& qos,
    const DDS::SubscriberQos& subscriberQos)
  {
    return endpoint_manager().update_subscription_qos(subscriptionId, qos, subscriberQos);
  }

  bool update_subscription_params(const GUID_t& subId, const DDS::StringSeq& params)
  {
    return endpoint_manager().update_subscription_params(subId, params);
  }

  void update_subscription_locators(const GUID_t& subId, const DCPS::TransportLocatorSeq& transInfo)
  {
    endpoint_manager().update_subscription_locators(subId, transInfo);
  }

  DDS::Subscriber_var bit_subscriber() const
  {
    return bit_subscriber_;
  }

protected:
  Sedp& endpoint_manager() { return *sedp_; }

  void remove_discovered_participant(DiscoveredParticipantIter& iter);

  void remove_discovered_participant_i(DiscoveredParticipantIter& iter);

#ifndef DDS_HAS_MINIMUM_BIT
  void enqueue_location_update_i(DiscoveredParticipantIter iter, DCPS::ParticipantLocation mask, const ACE_INET_Addr& from);
  void process_location_updates_i(DiscoveredParticipantIter& iter, bool force_publish = false);
  void publish_location_update_i(DiscoveredParticipantIter& iter);
#endif

  bool announce_domain_participant_qos();

  void type_lookup_service(const XTypes::TypeLookupService_rch type_lookup_service)
  {
    endpoint_manager().type_lookup_service(type_lookup_service);
  }

private:
#ifndef DDS_HAS_MINIMUM_BIT
  DCPS::ParticipantBuiltinTopicDataDataReaderImpl* part_bit()
  {
    DDS::Subscriber_var bit_sub(bit_subscriber());
    if (!bit_sub.in())
      return 0;

    DDS::DataReader_var d =
      bit_sub->lookup_datareader(DCPS::BUILT_IN_PARTICIPANT_TOPIC);
    return dynamic_cast<DCPS::ParticipantBuiltinTopicDataDataReaderImpl*>(d.in());
  }

  DCPS::ParticipantLocationBuiltinTopicDataDataReaderImpl* part_loc_bit()
  {
    DDS::Subscriber_var bit_sub(bit_subscriber());
    if (!bit_sub.in())
      return 0;

    DDS::DataReader_var d =
      bit_sub->lookup_datareader(DCPS::BUILT_IN_PARTICIPANT_LOCATION_TOPIC);
    return dynamic_cast<DCPS::ParticipantLocationBuiltinTopicDataDataReaderImpl*>(d.in());
  }

  DCPS::ConnectionRecordDataReaderImpl* connection_record_bit()
  {
    DDS::Subscriber_var bit_sub(bit_subscriber());
    if (!bit_sub.in())
      return 0;

    DDS::DataReader_var d =
      bit_sub->lookup_datareader(DCPS::BUILT_IN_CONNECTION_RECORD_TOPIC);
    return dynamic_cast<DCPS::ConnectionRecordDataReaderImpl*>(d.in());
  }

  DCPS::InternalThreadBuiltinTopicDataDataReaderImpl* internal_thread_bit()
  {
    DDS::Subscriber_var bit_sub(bit_subscriber());
    if (!bit_sub.in())
      return 0;

    DDS::DataReader_var d =
      bit_sub->lookup_datareader(DCPS::BUILT_IN_INTERNAL_THREAD_TOPIC);
    return dynamic_cast<DCPS::InternalThreadBuiltinTopicDataDataReaderImpl*>(d.in());
  }
#endif /* DDS_HAS_MINIMUM_BIT */

#ifdef OPENDDS_SECURITY
  typedef OPENDDS_MAP_CMP(GUID_t, DDS::Security::AuthRequestMessageToken, GUID_tKeyLessThan)
    PendingRemoteAuthTokenMap;
#endif

  void init(DDS::DomainId_t domain,
            DCPS::RepoId& guid,
            const DDS::DomainParticipantQos& qos,
            RtpsDiscovery* disco,
            XTypes::TypeLookupService_rch tls);

  mutable ACE_Thread_Mutex lock_;
  DDS::Subscriber_var bit_subscriber_;
  DDS::DomainParticipantQos qos_;
  DiscoveredParticipantMap participants_;
  RtpsDiscovery* disco_;
  DCPS::RcHandle<RtpsDiscoveryConfig> config_;

  // Participant:
  const DDS::DomainId_t domain_;
  DCPS::RepoId guid_;
  const DCPS::MonotonicTime_t participant_discovered_at_;
  bool is_application_participant_;

  void data_received(const DataSubmessage& data, const ParameterList& plist, const ACE_INET_Addr& from);

  void match_unauthenticated(const DCPS::RepoId& guid, DiscoveredParticipantIter& dp_iter);

  /// Get this participant's BIT data. user_data may be omitting depending on
  /// security settings.
  DDS::ParticipantBuiltinTopicData get_part_bit_data(bool secure) const;

  /**
   * If this is true participant user data should only be sent and received
   * securely, otherwise the user data should be empty and participant bit
   * updates should be withheld from the user.
   */
  bool secure_part_user_data() const;

  void update_rtps_relay_application_participant_i(DiscoveredParticipantIter iter);

#ifdef OPENDDS_SECURITY
  DDS::ReturnCode_t send_handshake_message(const DCPS::RepoId& guid,
                                           DiscoveredParticipant& dp,
                                           const DDS::Security::ParticipantStatelessMessage& msg);
  DCPS::MonotonicTimePoint schedule_handshake_resend(const DCPS::TimeDuration& time, const DCPS::RepoId& guid);
  bool match_authenticated(const DCPS::RepoId& guid, DiscoveredParticipantIter& iter);
  void attempt_authentication(const DiscoveredParticipantIter& iter, bool from_discovery);
  void update_agent_info(const DCPS::RepoId& local_guid, const ICE::AgentInfo& agent_info);
  void remove_agent_info(const DCPS::RepoId& local_guid);
#endif

  struct SpdpTransport
    : public virtual DCPS::RcEventHandler
    , public virtual DCPS::NetworkConfigListener
#ifdef OPENDDS_SECURITY
    , public ICE::Endpoint
#endif
  {
    typedef size_t WriteFlags;
    static const WriteFlags SEND_MULTICAST = (1 << 0);
    static const WriteFlags SEND_RELAY = (1 << 1);
    static const WriteFlags SEND_DIRECT = (1 << 2);

    class RegisterHandlers : public DCPS::ReactorInterceptor::Command {
    public:
      RegisterHandlers(const DCPS::RcHandle<SpdpTransport>& tport,
        const DCPS::ReactorTask_rch& reactor_task)
        : tport_(tport)
        , reactor_task_(reactor_task)
      {
      }

      void execute()
      {
        DCPS::RcHandle<SpdpTransport> tport = tport_.lock();
        if (!tport) {
          return;
        }
        tport->register_handlers(reactor_task_);
      }

    private:
      DCPS::WeakRcHandle<SpdpTransport> tport_;
      DCPS::ReactorTask_rch reactor_task_;
    };

    explicit SpdpTransport(DCPS::RcHandle<Spdp> outer);
    ~SpdpTransport();

    const ACE_SOCK_Dgram& choose_recv_socket(ACE_HANDLE h) const;

    virtual int handle_input(ACE_HANDLE h);

    void open(const DCPS::ReactorTask_rch& reactor_task);
    void register_unicast_socket(
      ACE_Reactor* reactor, ACE_SOCK_Dgram& socket, const char* what);
    void register_handlers(const DCPS::ReactorTask_rch& reactor_task);
    void enable_periodic_tasks();

    void shorten_local_sender_delay_i();
    void write(WriteFlags flags);
    void write_i(WriteFlags flags);
    void write_i(const DCPS::RepoId& guid, const ACE_INET_Addr& local_address, WriteFlags flags);
    void send(WriteFlags flags, const ACE_INET_Addr& local_address = ACE_INET_Addr());
    const ACE_SOCK_Dgram& choose_send_socket(const ACE_INET_Addr& addr) const;
    ssize_t send(const ACE_INET_Addr& addr);
    void close(const DCPS::ReactorTask_rch& reactor_task);
    void dispose_unregister();
    bool open_unicast_socket(u_short port_common, u_short participant_id);
#ifdef ACE_HAS_IPV6
    bool open_unicast_ipv6_socket(u_short port);
#endif

    void join_multicast_group(const DCPS::NetworkInterface& nic,
                              bool all_interfaces = false);
    void leave_multicast_group(const DCPS::NetworkInterface& nic);
    void add_address(const DCPS::NetworkInterface& interface,
                     const ACE_INET_Addr& address);
    void remove_address(const DCPS::NetworkInterface& interface,
                        const ACE_INET_Addr& address);

    ICE::Endpoint* get_ice_endpoint();

#ifdef OPENDDS_SECURITY
    ICE::AddressListType host_addresses() const;
    void send(const ACE_INET_Addr& address, const STUN::Message& message);
    ACE_INET_Addr stun_server_address() const;
  #ifndef DDS_HAS_MINIMUM_BIT
    void ice_connect(const ICE::GuidSetType& guids, const ACE_INET_Addr& addr);
    void ice_disconnect(const ICE::GuidSetType& guids, const ACE_INET_Addr& addr);
  #endif
#endif

    DCPS::WeakRcHandle<Spdp> outer_;
    Header hdr_;
    DataSubmessage data_;
    DCPS::SequenceNumber seq_;
    DCPS::TimeDuration lease_duration_;
    u_short uni_port_;
    ACE_SOCK_Dgram unicast_socket_;
    OPENDDS_STRING multicast_interface_;
    ACE_INET_Addr multicast_address_;
    ACE_SOCK_Dgram_Mcast multicast_socket_;
#ifdef ACE_HAS_IPV6
    u_short ipv6_uni_port_;
    ACE_SOCK_Dgram unicast_ipv6_socket_;
    OPENDDS_STRING multicast_ipv6_interface_;
    ACE_INET_Addr multicast_ipv6_address_;
    ACE_SOCK_Dgram_Mcast multicast_ipv6_socket_;
    OPENDDS_SET(OPENDDS_STRING) joined_ipv6_interfaces_;
#endif
    OPENDDS_SET(OPENDDS_STRING) joined_interfaces_;
    OPENDDS_SET(ACE_INET_Addr) send_addrs_;
    ACE_Message_Block buff_, wbuff_;
    typedef DCPS::PmfPeriodicTask<SpdpTransport> SpdpPeriodic;
    typedef DCPS::PmfSporadicTask<SpdpTransport> SpdpSporadic;
    typedef DCPS::PmfMultiTask<SpdpTransport> SpdpMulti;
    void send_local(const DCPS::MonotonicTimePoint& now);
    DCPS::RcHandle<SpdpMulti> local_sender_;
    void send_directed(const DCPS::MonotonicTimePoint& now);
    DCPS::RcHandle<SpdpSporadic> directed_sender_;
    OPENDDS_LIST(DCPS::RepoId) directed_guids_;
    void process_lease_expirations(const DCPS::MonotonicTimePoint& now);
    DCPS::RcHandle<SpdpSporadic> lease_expiration_processor_;
    DCPS::ThreadStatusManager* global_thread_status_manager_;
    DCPS::ThreadStatusManager local_thread_status_manager_;
    void thread_status_task(const DCPS::MonotonicTimePoint& now);
    DCPS::RcHandle<SpdpPeriodic> thread_status_sender_;
#ifdef OPENDDS_SECURITY
    void process_handshake_deadlines(const DCPS::MonotonicTimePoint& now);
    DCPS::RcHandle<SpdpSporadic> handshake_deadline_processor_;
    void process_handshake_resends(const DCPS::MonotonicTimePoint& now);
    DCPS::RcHandle<SpdpSporadic> handshake_resend_processor_;
    void send_relay(const DCPS::MonotonicTimePoint& now);
    DCPS::RcHandle<SpdpPeriodic> relay_sender_;
    void relay_stun_task(const DCPS::MonotonicTimePoint& now);
    DCPS::RcHandle<SpdpPeriodic> relay_stun_task_;
    ICE::ServerReflexiveStateMachine relay_srsm_;
    void process_relay_sra(ICE::ServerReflexiveStateMachine::StateChange);
    void disable_relay_stun_task();
#endif
    bool network_is_unreachable_;
    bool ice_endpoint_added_;

    DCPS::RelayMessageCounts relay_message_counts_;
  };

  DCPS::RcHandle<SpdpTransport> tport_;

  struct ChangeMulticastGroup : public DCPS::JobQueue::Job {
    enum CmgAction {CMG_JOIN, CMG_LEAVE};

    ChangeMulticastGroup(const DCPS::RcHandle<SpdpTransport>& tport,
                         const DCPS::NetworkInterface& nic, CmgAction action,
                         bool all_interfaces = false)
      : tport_(tport)
      , nic_(nic)
      , action_(action)
      , all_interfaces_(all_interfaces)
    {}

    void execute()
    {
      DCPS::RcHandle<SpdpTransport> tport = tport_.lock();
      if (!tport) {
        return;
      }

      if (action_ == CMG_JOIN) {
        tport->join_multicast_group(nic_, all_interfaces_);
      } else {
        tport->leave_multicast_group(nic_);
      }
    }

    DCPS::WeakRcHandle<SpdpTransport> tport_;
    DCPS::NetworkInterface nic_;
    CmgAction action_;
    bool all_interfaces_;
  };

#ifdef OPENDDS_SECURITY
  class SendStun : public DCPS::JobQueue::Job {
  public:
    SendStun(const DCPS::RcHandle<SpdpTransport>& tport,
             const ACE_INET_Addr& address,
             const STUN::Message& message)
      : tport_(tport)
      , address_(address)
      , message_(message)
    {}
    void execute();
  private:
    DCPS::WeakRcHandle<SpdpTransport> tport_;
    ACE_INET_Addr address_;
    STUN::Message message_;
  };

#ifndef DDS_HAS_MINIMUM_BIT
  class IceConnect : public DCPS::JobQueue::Job {
  public:
    IceConnect(DCPS::RcHandle<Spdp> spdp,
               const ICE::GuidSetType& guids,
               const ACE_INET_Addr& addr,
               bool connect)
      : spdp_(spdp)
      , guids_(guids)
      , addr_(addr)
      , connect_(connect)
    {}
    void execute();
  private:
    DCPS::RcHandle<Spdp> spdp_;
    ICE::GuidSetType guids_;
    ACE_INET_Addr addr_;
    bool connect_;
  };
#endif /* DDS_HAS_MINIMUM_BIT */
#endif

#ifdef ACE_HAS_CPP11
  std::atomic<bool> initialized_flag_; // Spdp initialized
#else
  ACE_Atomic_Op<ACE_Thread_Mutex, bool> initialized_flag_; // Spdp initialized
#endif

  bool eh_shutdown_;
  DCPS::ConditionVariable<ACE_Thread_Mutex> shutdown_cond_;
#ifdef ACE_HAS_CPP11
  std::atomic<bool> shutdown_flag_; // Spdp shutting down
#else
  ACE_Atomic_Op<ACE_Thread_Mutex, bool> shutdown_flag_; // Spdp shutting down
#endif

  void get_discovered_participant_ids(DCPS::RepoIdSet& results) const;

  BuiltinEndpointSet_t available_builtin_endpoints_;
  DCPS::RcHandle<Sedp> sedp_;

  typedef OPENDDS_MULTIMAP(DCPS::MonotonicTimePoint, DCPS::RepoId) TimeQueue;

  void remove_lease_expiration_i(DiscoveredParticipantIter iter);
  void update_lease_expiration_i(DiscoveredParticipantIter iter,
                                 const DCPS::MonotonicTimePoint& now);
  void process_lease_expirations(const DCPS::MonotonicTimePoint& now);
  TimeQueue lease_expirations_;

#ifdef OPENDDS_SECURITY
  DDS::Security::ExtendedBuiltinEndpointSet_t available_extended_builtin_endpoints_;
  Security::SecurityConfig_rch security_config_;
  bool security_enabled_;

  DCPS::SequenceNumber stateless_sequence_number_;

  DDS::Security::IdentityHandle identity_handle_;
  DDS::Security::PermissionsHandle permissions_handle_;
  DDS::Security::ParticipantCryptoHandle crypto_handle_;

  DDS::Security::IdentityToken identity_token_;
  DDS::Security::IdentityStatusToken identity_status_token_;
  DDS::Security::PermissionsToken permissions_token_;
  DDS::Security::PermissionsCredentialToken permissions_credential_token_;

  DDS::Security::ParticipantSecurityAttributes participant_sec_attr_;

  void start_ice(ICE::Endpoint* endpoint, DCPS::RepoId remote, BuiltinEndpointSet_t avail,
                 DDS::Security::ExtendedBuiltinEndpointSet_t extended_avail,
                 const ICE::AgentInfo& agent_info);
  void stop_ice(ICE::Endpoint* endpoint, DCPS::RepoId remote, BuiltinEndpointSet_t avail,
                DDS::Security::ExtendedBuiltinEndpointSet_t extended_avail);

  void purge_handshake_deadlines(DiscoveredParticipantIter iter);
  TimeQueue handshake_deadlines_;

  void purge_handshake_resends(DiscoveredParticipantIter iter);
  TimeQueue handshake_resends_;

  size_t n_participants_in_authentication_;
  void set_auth_state(DiscoveredParticipant& dp, AuthState state);
#endif

  void erase_participant(DiscoveredParticipantIter iter);

  friend class ::DDS_TEST;
};

} // namespace RTPS
} // namespace OpenDDS

OPENDDS_END_VERSIONED_NAMESPACE_DECL

#endif // OPENDDS_RTPS_SPDP_H
