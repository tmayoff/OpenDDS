/*
 * Distributed under the OpenDDS License.
 * See: http://www.opendds.org/license.html
 */

#include "MessageTypeSupportImpl.h"
#include "Writer.h"

#include <dds/DCPS/Marked_Default_Qos.h>

#include <ace/Get_Opt.h>
#include <ace/Log_Msg.h>
#include <ace/OS_NS_stdlib.h>
#include <ace/OS_NS_unistd.h>


int ACE_TMAIN(int argc, ACE_TCHAR *argv[])
{
  int status = 0;

  try {
    // Initialize DomainParticipantFactory
    DDS::DomainParticipantFactory_var dpf =
      TheParticipantFactoryWithArgs(argc, argv);

    // Create DomainParticipant
    DDS::DomainParticipantQos participant_qos;
    dpf->get_default_participant_qos(participant_qos);
    DDS::DomainParticipant_var participant =
      dpf->create_participant(113,
                              participant_qos,
                              DDS::DomainParticipantListener::_nil(),
                              OpenDDS::DCPS::DEFAULT_STATUS_MASK);
    if (CORBA::is_nil(participant.in())) {
      ACE_ERROR_RETURN((LM_ERROR,
                        ACE_TEXT("%N:%l: main()")
                        ACE_TEXT(" ERROR: create_participant failed!\n")),
                       1);
    }

    Messenger::ImageTypeSupport_var its = new Messenger::ImageTypeSupportImpl();

    if (its->register_type(participant.in(), "") != DDS::RETCODE_OK) {
      ACE_ERROR_RETURN((LM_ERROR,
                        ACE_TEXT("%N:%l: main()")
                        ACE_TEXT(" ERROR: register_type failed!\n")),
                       1);
    }

    // Create Topic
    CORBA::String_var type_name = its->get_type_name();
    DDS::Topic_var topic =
      participant->create_topic("Movie Discussion List",
                                type_name.in(),
                                TOPIC_QOS_DEFAULT,
                                DDS::TopicListener::_nil(),
                                OpenDDS::DCPS::DEFAULT_STATUS_MASK);

    if (CORBA::is_nil(topic.in())) {
      ACE_ERROR_RETURN((LM_ERROR,
                        ACE_TEXT("%N:%l: main()")
                        ACE_TEXT(" ERROR: create_topic failed!\n")),
                       1);
    }

    // Create Publisher
    DDS::Publisher_var pub =
      participant->create_publisher(PUBLISHER_QOS_DEFAULT,
                                    DDS::PublisherListener::_nil(),
                                    OpenDDS::DCPS::DEFAULT_STATUS_MASK);

    if (CORBA::is_nil(pub.in())) {
      ACE_ERROR_RETURN((LM_ERROR,
                        ACE_TEXT("%N:%l: main()")
                        ACE_TEXT(" ERROR: create_publisher failed!\n")),
                       1);
    }

    DDS::DataWriterQos qos;
    pub->get_default_datawriter_qos(qos);
    qos.liveliness.kind = DDS::AUTOMATIC_LIVELINESS_QOS;
    qos.liveliness.lease_duration.sec = 5;
    qos.liveliness.lease_duration.nanosec = 0;
    qos.history.kind = DDS::KEEP_ALL_HISTORY_QOS;
    qos.durability.kind = DDS::TRANSIENT_LOCAL_DURABILITY_QOS;

  } catch (const CORBA::Exception& e) {
    e._tao_print_exception("Exception caught in main():");
    status = 1;
  }

  ACE_DEBUG((LM_DEBUG, "Publisher exiting with status=%d\n", status));
  return status;
}
