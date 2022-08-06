#include "MessageTypeSupportImpl.h"

#include <ace/Log_Msg.h>

#include <dds/DCPS/Marked_Default_Qos.h>
#include <dds/DdsDcpsInfrastructureTypeSupportC.h>
#include <dds/DCPS/Service_Participant.h>

int ACE_TMAIN (int argc, ACE_TCHAR *argv[]) {
    DDS::DomainParticipantFactory_var dpf = TheParticipantFactoryWithArgs(argc, argv);

    DDS::DomainParticipant_var participant = dpf->create_participant(42, PARTICIPANT_QOS_DEFAULT, 0, OpenDDS::DCPS::DEFAULT_STATUS_MASK);
}