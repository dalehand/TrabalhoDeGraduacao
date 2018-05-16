#include <stdio.h>
#include <string.h>
#include <math.h>
#include <dos.h>
#include "DeviceNet.h"

UCHAR global_CAN_buf[BUFSIZE];      // Buffer with CAN data
UINT global_timer[10];              // Global timers

class CONNECTION
{
private:
    static UINT class_revision;
    static UCHAR path[PATH_SIZE];
    UCHAR state, instance, instance_type, transport_class_trigger; //required instance attributes (CIP v1-3.13)
    UINT produced_connection_id, consumed_connection_id;
    UCHAR initial_comm_characteristics;
    UINT produced_connection_size, consumed_connection_size;
    UINT expected_packet_rate;
    UCHAR watchdog_timeout_action;
    UINT produced_connection_path_length;
    UCHAR produced_connection_path[PATH_SIZE];
    UINT consumed_connection_path_length;
    UCHAR consumed_connection_path[PATH_SIZE];
    UINT production_inhibit_time;

public:
    UCHAR get_state(void);
    void set_state(UCHAR);
    UCHAR get_timeout_action(void);
    CONNECTION(UCHAR inst)
    {
        instance = inst;
        if (instance == EXPLICIT) instance_type =0; //(CIP v1-3.15) instance type = 00 => Explicit Message; instance type = 01 => I/O Message
        else instance_type=1;                       //(CIP v1-3.15)
        set_state(NON_EXISTENT);
    }

};

UCHAR CONNECTION::get_state(void)
{
    return state;
}

UCHAR CONNECTION::get_timeout_action(void)
{
    return watchdog_timeout_action;
}


void CONNECTION::set_state(UCHAR st)
{
    state = st;
    if((instance == EXPLICIT)&&(state == ESTABLISHED))
    {
        transport_class_trigger = 0x83; // 1 (Dir=Serv) 000 (ProdTrig=Cyclic) 0011 (TranspClass=3 do not prepend a 16-bit sequence CIPv3-3.4) CIPv1-3.27
        produced_connection_id = 0x5FB; // MessageGroup2 CIPv3-3.4 CIPv3-2.4
        consumed_connection_id = 0x5FC; // MessageGroup2 CIPv3-3.4 CIPv3-2.4
        initial_comm_characteristics = 0x21; // 2 = Produce across message group 2(source); 1 = Consume across message group 2(destination) CIPv3-6/7
        produced_connection_size = 0xFFFF; // Message router maximum amount of data
        consumed_connection_size = 0xFFFF; // Message router maximum amount of data
        expected_packet_rate = 2500; // Default value (CIPv3-3.30)
        watchdog_timeout_action = 0x01; // auto delete (CIPv3-3.30)

    }
    if((instance == IO_POLL)&&(state == CONFIGURING))
    {

        transport_class_trigger = 0x82; // 1 (Dir=Serv) 000 (ProdTrig=Cyclic) 0010 (TranspClass=2 do not prepend a 16-bit sequence CIPv3-3.4) CIPv1-3.27
        produced_connection_id = 0x3FF; // MessageGroup1 CIPv3-3.4 CIPv3-2.4
        consumed_connection_id = 0x5FD; // MessageGroup2 CIPv3-3.4 CIPv3-2.4
        initial_comm_characteristics = 0x01; // 0 = Produce across message group 1; 1 = Consume across message group 2(destination) CIPv3-6/7
        produced_connection_size = 3;            // i/o poll response length
        consumed_connection_size = 0;
        expected_packet_rate = 0; // Default value (CIPv3-3.30)
        watchdog_timeout_action = 0x00; // go to timed-out (CIPv3-3.30)

    }
    if (state == NON_EXISTENT)
    {
        global_timer[instance] = 0;   // stop connection timer
    }
}

// Handles Explicit request to the class
void CONNECTION::handle_class_inquiry(UCHAR request[], UCHAR response[])
{
    UINT service, attrib, error;

    service = request[1];
    attrib = request[4];
    error = 0;
    memset(response, 0, BUFSIZE);

    switch(service)
    {
        case GET_REQUEST:
        switch(attrib)
        {
            case 1:  // get revision attribute
            response[0] = request[0] & NON_FRAGMENTED;
            response[1] = service | SUCCESS_RESPONSE;
            response[2] = LOBYTE(class_revision);
            response[3] = HIBYTE(class_revision);
            response[LENGTH] = 4;
            break;

            default:
            error = ATTRIB_NOT_SUPPORTED;
            break;
        }
        break;


        default:
        error = SERVICE_NOT_SUPPORTED;
        break;
    }

    if (error)
    {
        response[0] = request[0] & NON_FRAGMENTED;
        response[1] = ERROR_RESPONSE;
        response[2] = error;
        response[3] = NO_ADDITIONAL_CODE;
        response[LENGTH] = 4;
    }
}

void CONNECTION::handle_explicit(UCHAR request[], UCHAR response[])
{
    UINT service, attrib, error;
    UINT test_val, temp;					// for EPR calculation

    service = request[1];
    attrib = request[4];
    error = 0;
    memset(response, 0, BUFSIZE);

    switch(service)
    {
        case GET_REQUEST:
        switch(attrib)
        {
            case 1:   // connection state
            response[0] = request[0] & NON_FRAGMENTED;
            response[1] = service | SUCCESS_RESPONSE;
            response[2] = state;
            response[LENGTH] = 3;
            break;

            case 2:   // instance type
            response[0] = request[0] & NON_FRAGMENTED;
            response[1] = service | SUCCESS_RESPONSE;
            response[2] = instance_type;
            response[LENGTH] = 3;
            break;

            case 3:   // transport class trigger
            response[0] = request[0] & NON_FRAGMENTED;
            response[1] = service | SUCCESS_RESPONSE;
            response[2] = transport_class_trigger;
            response[LENGTH] = 3;
            break;

            case 4:   // produced connection id
            response[0] = request[0] & NON_FRAGMENTED;
            response[1] = service | SUCCESS_RESPONSE;
            response[2] = LOBYTE(produced_connection_id);
            response[3] = HIBYTE(produced_connection_id);
            response[LENGTH] = 4;
            break;

            case 5:   // consumed connection id
            response[0] = request[0] & NON_FRAGMENTED;
            response[1] = service | SUCCESS_RESPONSE;
            response[2] = LOBYTE(consumed_connection_id);
            response[3] = HIBYTE(consumed_connection_id);
            response[LENGTH] = 4;
            break;

            case 6:   // initial comm characteristics
            response[0] = request[0] & NON_FRAGMENTED;
            response[1] = service | SUCCESS_RESPONSE;
            response[2] = initial_comm_characteristics;
            response[LENGTH] = 3;
            break;

            case 7:   // produced connection size
            response[0] = request[0] & NON_FRAGMENTED;
            response[1] = service | SUCCESS_RESPONSE;
            response[2] = LOBYTE(produced_connection_size);
            response[3] = HIBYTE(produced_connection_size);
            response[LENGTH] = 4;
            break;

            case 8: 	// consumed connection size
            response[0] = request[0] & NON_FRAGMENTED;
            response[1] = service | SUCCESS_RESPONSE;
            response[2] = LOBYTE(consumed_connection_size);
            response[3] = HIBYTE(consumed_connection_size);
            response[LENGTH] = 4;
            break;

            case 9: // EPR
            response[0] = request[0] & NON_FRAGMENTED;
            response[1] = service | SUCCESS_RESPONSE;
            response[2] = LOBYTE(expected_packet_rate);
            response[3] = HIBYTE(expected_packet_rate);
            response[LENGTH] = 4;
            break;

            case 12:   // watchdog timeout action
            response[0] = request[0] & NON_FRAGMENTED;
            response[1] = service | SUCCESS_RESPONSE;
            response[2] = watchdog_timeout_action;
            response[LENGTH] = 3;
            break;

            case 13: // produced conxn path length
            response[0] = request[0] & NON_FRAGMENTED;
            response[1] = service | SUCCESS_RESPONSE;
            response[2] = LOBYTE(produced_connection_path_length);
            response[3] = HIBYTE(produced_connection_path_length);
            response[LENGTH] = 4;
            break;

            case 14:   // produced conxn path
            response[0] = request[0] & NON_FRAGMENTED;
            response[1] = service | SUCCESS_RESPONSE;
            if (instance_type == EXPLICIT_TYPE)
            {
                // Path is NULL for Explicit Connection
                response[LENGTH] = 2;
            }
            else  // Path for data sent in I/O Poll response
            {
                memcpy(&response[2], produced_connection_path, 6);
                response[LENGTH] = 8;
            }
            break;

            case 15: // consumed conxn path length
            response[0] = request[0] & NON_FRAGMENTED;
            response[1] = service | SUCCESS_RESPONSE;
            response[2] = LOBYTE(consumed_connection_path_length);
            response[3] = HIBYTE(consumed_connection_path_length);
            response[LENGTH] = 4;
            break;

            case 16:  // consumed conxn path
            response[0] = request[0] & NON_FRAGMENTED;
            response[1] = service | SUCCESS_RESPONSE;
            // path is NULL for both Explicit and I/O Poll Connections
            response[LENGTH] = 2;
            break;

            case 17:  // production inhibit time
            response[0] = request[0] & NON_FRAGMENTED;
            response[1] = service | SUCCESS_RESPONSE;
            response[2] = LOBYTE(production_inhibit_time);
            response[3] = HIBYTE(production_inhibit_time);
            response[LENGTH] = 4;
            break;


            default:
            error = ATTRIB_NOT_SUPPORTED;
            break;
        }
        break;


        case SET_REQUEST:
        switch(attrib)
        {
            case 1:
            case 2:
            case 3:
            case 4:
            case 5:
            case 6:
            case 7:
            case 8:
            case 13:
            case 14:
            case 15:
            case 16:
            case 17:
            error = ATTRIB_NOT_SETTABLE;
            break;

            case 9: // Set EPR
            test_val = request[5] + 256 * request[6];

            // Limit EPR so rounding up doesn't take it over 65,535
            if (test_val > 65000) error = INVALID_ATTRIB_VALUE;
            else
            {
                // This tweaks the EPR to be a multiple of 50
                temp = test_val / 50;
                if ((temp * 50) != test_val) temp++;   // round up
                expected_packet_rate = temp * 50;

                // see if I/O Poll connection should be established
                if ((instance == IO_POLL) && (state == CONFIGURING))
                {
                    set_state(ESTABLISHED);
                    // IF EPR > 2500 use it, else use 2500 (200 counts)
                    // This ensures that timeout time is at least 10 seconds
                    if ((expected_packet_rate > 2500) ||
                         (expected_packet_rate == 0))
                    {
                        global_timer[IO_POLL] = (expected_packet_rate / 50) * 4;
                    }
                    else global_timer[IO_POLL] = 200;
                }
                else global_timer[instance] = (expected_packet_rate / 50) * 4;

                response[0] = request[0] & NON_FRAGMENTED;
                response[1] = service | SUCCESS_RESPONSE;
                response[2] = LOBYTE(expected_packet_rate);
                response[3] = HIBYTE(expected_packet_rate);
                response[LENGTH] = 4;
            }
            break;

            case 12:  // Set watchdog timeout action
            if (instance == IO_POLL) error = ATTRIB_NOT_SETTABLE;
            else if ((request[5] != 1) &&
                        (request[5] != 3)) error = INVALID_ATTRIB_VALUE;
            else
            {
                watchdog_timeout_action = request[5];
                response[0] = request[0] & NON_FRAGMENTED;
                response[1] = service | SUCCESS_RESPONSE;
                response[LENGTH] = 2;
            }
            break;


            default:
            error = ATTRIB_NOT_SUPPORTED;
            break;
        }
        break;

        default:
        error = SERVICE_NOT_SUPPORTED;
        break;
    }

    if (error)
    {
        response[0] = request[0] & NON_FRAGMENTED;
        response[1] = ERROR_RESPONSE;
        response[2] = error;
        response[3] = NO_ADDITIONAL_CODE;
        response[LENGTH] = 4;
    }
}

int CONNECTION::link_consumer(UCHAR request[])
{
    UCHAR i, length, fragment_count, fragment_type;

    if ((instance == EXPLICIT) && (state == DEFERRED)) state = ESTABLISHED;
    if (state != ESTABLISHED) return NO_RESPONSE;

    global_timer[instance] = (expected_packet_rate / 50) * 4;    // restart connxn timer

    // Handle an I/O Poll request
    if (instance == IO_POLL)
    {
        // The Master should not send me any data in I/O message
        if (request[LENGTH] == 0) return OK;
        else return NO_RESPONSE;

    }
    // From this point on we are dealing with an Explicit message
    // If it is not fragmented, reset frag counters to initial state
    // and process the current incoming message.
    if ((request[0] & 0x80) == 0)
    {
        rcve_index = 0;
        xmit_index = 0;
        my_rcve_fragment_count = 0;
        my_xmit_fragment_count = 0;
        ack_timeout_counter = 0;
        global_timer[ACK_WAIT] = 0;
        return OK;
    }

    // At this point we are dealing with either an explicit fragment
    // or an ACK to an explicit message we sent earlier
    fragment_type =  request[1] & 0xC0;
    fragment_count = request[1] & 0x3F;
    length = request[LENGTH];


    switch(fragment_type)
    {
        case ACK_FRAG:
        // Received an ack from the master to an explicit fragment I sent earlier
        // Send the request to the link producer along with a tag so it knows
        // what do do with the message
        request[MESSAGE_TAG] = RECEIVED_ACK;
        link_producer(request);
        break;


        case FIRST_FRAG:
        if (fragment_count != 0)
        {
            // Reset fragment counters to initial state and drop fragment
            rcve_index = 0;
            my_rcve_fragment_count = 0;
            return NO_RESPONSE;
        }

        // get rid of any existing fragments and process first fragment
        rcve_index = 0;
        my_rcve_fragment_count = 0;
        memset(rcve_fragment_buf, 0, BUFSIZE);
        rcve_fragment_buf[rcve_index] = request[0] & 0x7F; // remove fragment flag
        rcve_index++;
        for (i = 2; i < length; i++)      // do not copy fragment info
        {
            rcve_fragment_buf[rcve_index] = request[i];
            rcve_index++;
        }

        // Check to see if Master has exceeded byte count limit
        if (rcve_index > consumed_conxn_size)
        {
            rcve_index = 0;                       // reset fragment counters
            my_rcve_fragment_count = 0;
            request[MESSAGE_TAG] = ACK_ERROR;
            link_producer(request);
        }
        else  // fragment checked out OK so send ACK
        {
            request[MESSAGE_TAG] = SEND_ACK;
            link_producer(request);
            my_rcve_fragment_count++;
        }
        break;



        case MIDDLE_FRAG:
        if (my_rcve_fragment_count == 0) return NO_RESPONSE;  // just drop fragment

        // See if Master has sent the same fragment again.  If so just send ACK
        if (fragment_count == (my_rcve_fragment_count - 1))
        {
             request[MESSAGE_TAG] = SEND_ACK;
             link_producer(request);
        }
        // See if received fragment count does not agree with my count
        // If so, reset to beginning
        else if (fragment_count != my_rcve_fragment_count)
        {
                rcve_index = 0;
                my_rcve_fragment_count = 0;
        }

        else  // Fragment was validated so process it
        {
            // Copy the fragment to reassembly buffer, omit first 2 bytes
            for (i = 2; i < length; i++)
            {
                rcve_fragment_buf[rcve_index] = request[i];
                rcve_index++;
            }
            // Check to see if Master has exceeded byte count limit
            if (rcve_index > consumed_conxn_size)
            {
                rcve_index = 0;
                my_rcve_fragment_count = 0;
                request[MESSAGE_TAG] = ACK_ERROR;
                link_producer(request);
            }
            else  // send good ACK back to Master
            {
                request[MESSAGE_TAG] = SEND_ACK;
                link_producer(request);
                my_rcve_fragment_count++;
            }
        }
        break;



        case LAST_FRAG:
        if (my_rcve_fragment_count == 0) return NO_RESPONSE;  // just drop fragment


        // See if received fragment count does not agree with my count
        // If so, reset to beginning
        else if (fragment_count != my_rcve_fragment_count)
        {
                rcve_index = 0;
                my_rcve_fragment_count = 0;
        }
        else  // Fragment was validated, so process it
        {
            // Copy the fragment to reassembly buffer, omit first 2 bytes
            for (i = 2; i < length; i++)
            {
                rcve_fragment_buf[rcve_index] = request[i];
                rcve_index++;
            }

            // Check to see if Master has exceeded byte count limit
            if (rcve_index > consumed_conxn_size)
            {
                rcve_index = 0;
                my_rcve_fragment_count = 0;
                memset(rcve_fragment_buf, 0, BUFSIZE);
                request[MESSAGE_TAG] = ACK_ERROR;
                link_producer(request);
            }
            else  // Fragment was OK, send ACK and reset everything
            {
                request[MESSAGE_TAG] = SEND_ACK;
                link_producer(request);
                // We are done receiving a fragmented explicit message
                // Copy from temporary buffer back to request buffer
                // and process complete Explicit message
                memcpy(request, rcve_fragment_buf, BUFSIZE);
                request[LENGTH] = rcve_index;
                rcve_index = 0;
                my_rcve_fragment_count = 0;
                memset(rcve_fragment_buf, 0, BUFSIZE);
                return OK;
            }
        }
        break;


        default:
        break;
    }

    return NO_RESPONSE;
}

class DISCRETE_INPUT_POINT
{
private:
    UCHAR value;
    UCHAR instance;
    BOOL status;
    ULONG sensor_clock;
    static UINT class_revision;
public:
    static void handle_class_inquiry(UCHAR*, UCHAR*);
    void handle_explicit(UCHAR*, UCHAR*);
    UCHAR get_value(void);
    void init_obj(UCHAR);
};

void DISCRETE_INPUT_POINT::init_obj(UCHAR inst)
{
    instance = inst;
    sensor_clock= 0;
    value = 0;
    status = 0;
}

void DISCRETE_INPUT_POINT::handle_class_inquiry(UCHAR request[], UCHAR response[])
{
    UINT service, attrib, error;

    service = request[1];
    attrib = request[4];
    error = 0;
    memset(response, 0, BUFSIZE);

    switch(service)
    {
        case GET_REQUEST:
        switch(attrib)
        {
            case 1:  // get revision attribute
            response[0] = request[0] & NON_FRAGMENTED;
            response[1] = service | SUCCESS_RESPONSE;
            response[2] = LOBYTE(class_revision);
            response[3] = HIBYTE(class_revision);
            response[LENGTH] = 4;
            break;

            default:
            error = ATTRIB_NOT_SUPPORTED;
            break;
        }
        break;


        default:
        error = SERVICE_NOT_SUPPORTED;
        break;
    }

    if (error)
    {
        response[0] = request[0] & NON_FRAGMENTED;
        response[1] = ERROR_RESPONSE;
        response[2] = error;
        response[3] = NO_ADDITIONAL_CODE;
        response[LENGTH] = 4;
    }

}

void DISCRETE_INPUT_POINT::handle_explicit(UCHAR request[], UCHAR response[])
{
    UINT service, attrib, error;

    service = request[1];
    attrib = request[4];
    error = 0;
    memset(response, 0, BUFSIZE);

    switch(service)
    {
        case GET_REQUEST:
        switch(attrib)
        {
            case 3:  // value
            response[0] = request[0] & NON_FRAGMENTED;
            response[1] = service | SUCCESS_RESPONSE;
            response[2] = value;
            response[LENGTH] = 3;
            break;

            case 4:  // status
            response[0] = request[0] & NON_FRAGMENTED;
            response[1] = service | SUCCESS_RESPONSE;
            response[2] = (UCHAR)status;
            response[LENGTH] = 3;
            break;

            default:
            error = ATTRIB_NOT_SUPPORTED;
            break;
        }
        break;

        default:
        error = SERVICE_NOT_SUPPORTED;
        break;
    }

    if (error)
    {
        response[0] = request[0] & NON_FRAGMENTED;
        response[1] = ERROR_RESPONSE;
        response[2] = error;
        response[3] = NO_ADDITIONAL_CODE;
        response[LENGTH] = 4;
    }
}

UCHAR DISCRETE_INPUT_POINT::get_value(void)
{
    return value;
}



class DISCRETE_OUTPUT_POINT
{
private:
    UCHAR value;
    UCHAR instance;
    BOOL status;
    static UINT class_revision;
public:
    static void handle_class_inquiry(UCHAR*, UCHAR*);
    void handle_explicit(UCHAR*, UCHAR*);
    void init_obj(UCHAR);
};

void DISCRETE_OUTPUT_POINT::init_obj(UCHAR inst)
{
    instance = inst;
    value = 0;
    status = 0;
}

void DISCRETE_OUTPUT_POINT::handle_class_inquiry(UCHAR request[], UCHAR response[])
{
    UINT service, attrib, error;

    service = request[1];
    attrib = request[4];
    error = 0;
    memset(response, 0, BUFSIZE);

    switch(service)
    {
        case GET_REQUEST:
        switch(attrib)
        {
            case 1:  // get revision attribute
            response[0] = request[0] & NON_FRAGMENTED;
            response[1] = service | SUCCESS_RESPONSE;
            response[2] = LOBYTE(class_revision);
            response[3] = HIBYTE(class_revision);
            response[LENGTH] = 4;
            break;

            default:
            error = ATTRIB_NOT_SUPPORTED;
            break;
        }
        break;


        default:
        error = SERVICE_NOT_SUPPORTED;
        break;
    }

    if (error)
    {
        response[0] = request[0] & NON_FRAGMENTED;
        response[1] = ERROR_RESPONSE;
        response[2] = error;
        response[3] = NO_ADDITIONAL_CODE;
        response[LENGTH] = 4;
    }

}

void DISCRETE_OUTPUT_POINT::handle_explicit(UCHAR request[], UCHAR response[])
{
    UINT service, attrib, error;

    service = request[1];
    attrib = request[4];
    error = 0;
    memset(response, 0, BUFSIZE);

    switch(service)
    {
        case GET_REQUEST:
        switch(attrib)
        {
            case 4:  // status
            response[0] = request[0] & NON_FRAGMENTED;
            response[1] = service | SUCCESS_RESPONSE;
            response[2] = (UCHAR)status;
            response[LENGTH] = 3;
            break;

            default:
            error = ATTRIB_NOT_SUPPORTED;
            break;
        }
        break;

        case SET_REQUEST:
        switch(attrib)
        {
            case 3: // set value
            response[0] = request[0] & NON_FRAGMENTED;
            response[1] = service | SUCCESS_RESPONSE;
            response[LENGTH] = 2;
        }

        default:
        error = SERVICE_NOT_SUPPORTED;
        break;
    }

    if (error)
    {
        response[0] = request[0] & NON_FRAGMENTED;
        response[1] = ERROR_RESPONSE;
        response[2] = error;
        response[3] = NO_ADDITIONAL_CODE;
        response[LENGTH] = 4;
    }
}

class DEVICENET  // DeviceNet class
{
    private:
    static UINT class_revision;
    // declare pointers of type CONNECTION to allow
    // sending messages to the connection objects
    CONNECTION *explicit_conxn, *io_poll_conxn;

    public:
    UCHAR mac_id;
    ULONG serial;
    UCHAR baud_rate;
    UINT vendor_id;
    BOOL bus_off_int;
    struct
    {
        UCHAR choice;
        UCHAR my_master;
    } allocation;
    UCHAR physical_port;
    static void handle_class_inquiry(UCHAR*, UCHAR*);
    int handle_unconnected_port(UCHAR*, UCHAR*);
    void handle_timeout(UCHAR);
    void handle_explicit(UCHAR*, UCHAR*);
    int consume_dup_mac(UCHAR*);
    void send_dup_mac_response(void);
    DEVICENET(UCHAR mac, UCHAR baud, UINT id, ULONG ser, CONNECTION *exp, CONNECTION *io)
    {
        explicit_conxn = exp;
        io_poll_conxn = io;
        mac_id = mac;
        baud_rate = baud;
        vendor_id = id;
        serial = ser;
        allocation.choice = 0;                      		// no connections
        allocation.my_master = DEFAULT_MASTER_MAC_ID;  	// device not yet allocated
        bus_off_int = 0;
        physical_port = 0;
    }
};




// Handle Explicit request to class
void DEVICENET::handle_class_inquiry(UCHAR request[], UCHAR response[])
{
    UINT service, attrib, error;

    service = request[1];
    attrib = request[4];
    error = 0;
    memset(response, 0, BUFSIZE);

    switch(service)
    {
        case GET_REQUEST:
        switch(attrib)
        {
            case 1:  // get revision attribute
            response[0] = request[0] & NON_FRAGMENTED;
            response[1] = service | SUCCESS_RESPONSE;
            response[2] = LOBYTE(class_revision);
            response[3] = HIBYTE(class_revision);
            response[LENGTH] = 4;
            break;

            default:
            error = ATTRIB_NOT_SUPPORTED;
            break;
        }
        break;


        default:
        error = SERVICE_NOT_SUPPORTED;
        break;
    }

    if (error)
    {
        response[0] = request[0] & NON_FRAGMENTED;
        response[1] = ERROR_RESPONSE;
        response[2] = error;
        response[3] = NO_ADDITIONAL_CODE;
        response[LENGTH] = 4;
    }
}




// When a connection timer expires, this function transitions the connection
// to the appropriate state.  After this, if neither connection is in
// the established state, it releases all connections and the device is no
// longer "owned" by the master.
void DEVICENET::handle_timeout(UCHAR conxn)
{
    UCHAR timeout_action;

    if (conxn == EXPLICIT_CONXN)
    {
      timeout_action = explicit_conxn->get_timeout_action();
        // If in auto-delete mode, or if in deferred delete mode
      // and I/O Poll conxn is not established, delete Explicit conxn.
        if ((timeout_action == 1) ||
            ((timeout_action == 3) && (io_poll_conxn->get_state() != ESTABLISHED)))
        {
            allocation.choice &= (~EXPLICIT_CONXN);
            explicit_conxn->set_state(NON_EXISTENT);
      }
        // If in deferred-delete mode and I/O Poll conxn is established
        // go to deferred state (still considered allocated)
        else if ((timeout_action == 3) && (io_poll_conxn->get_state() == ESTABLISHED))
        {
            explicit_conxn->set_state(DEFERRED);
        }
    }

    if (conxn == IO_POLL_CONXN)
    {
        allocation.choice &= (~IO_POLL_CONXN);
        io_poll_conxn->set_state(TIMED_OUT);
        // If Explicit is in deferred state, delete Explicit connnection
        if (explicit_conxn->get_timeout_action() == 3)
        {
            allocation.choice &= (~EXPLICIT_CONXN);
            explicit_conxn->set_state(NON_EXISTENT);
        }
    }

    // If neither connection is in established state, release everything
    if ((explicit_conxn->get_state() != ESTABLISHED) &&
        (io_poll_conxn->get_state() != ESTABLISHED))
    {
        explicit_conxn->set_state(NON_EXISTENT);
        io_poll_conxn->set_state(NON_EXISTENT);
        allocation.choice = 0;
        global_status &= (~OWNED);
        allocation.my_master = DEFAULT_MASTER_MAC_ID;
    }
}



// Handles a request to the unconnected port.  This will be a request
// from the Master to allocate or release connections.
int DEVICENET::handle_unconnected_port(UCHAR request[], UCHAR response[])
{
    UINT service, error;
    UCHAR additional_code, master_mac_id, alloc_request, release_request;

    service = request[1];
    error = 0;
    additional_code = 0;
    memset(response, 0, BUFSIZE);

    // Note:  In this context, "allocated" means that a connection is in
    // either the CONFIGURING, ESTABLISHED, or DEFERRED states, as opposed
    // to being in the NON_EXISTENT or TIMED_OUT states

    switch(service)
    {
        case ALLOCATE_CONNECTIONS:      // request to allocate connections
        // first validate the request
        alloc_request = request[4];
        master_mac_id = request[5] & 0x3F;
        if ((master_mac_id != allocation.my_master) &&
             (allocation.my_master != DEFAULT_MASTER_MAC_ID))
        {
            error = OBJECT_STATE_CONFLICT;
            additional_code = ALLOCATION_CONFLICT;
        }
        else if (alloc_request == 0)   	// master must allocate something
        {
            error = INVALID_ATTRIB_VALUE;
            additional_code = INVALID_ALLOC_CHOICE;
        }
        // see if the master sets ack bit but neither COS nor Cyclic are set
        else if ((alloc_request & 0x40) && ((alloc_request & 0x30) == 0))
        {
            error = INVALID_ATTRIB_VALUE;
            additional_code = INVALID_ALLOC_CHOICE;
        }
        // see if master tries to allocate something not supported
        else if (alloc_request & 0xFC)
        {
            error = RESOURCE_UNAVAILABLE;
            additional_code = INVALID_ALLOC_CHOICE;
        }
        // Master must at least allocate explicit unless it is already allocated
        else if (((alloc_request & EXPLICIT_CONXN) == 0) &&
                  ((allocation.choice & EXPLICIT_CONXN) == 0))
        {
            error = INVALID_ATTRIB_VALUE;
            additional_code = INVALID_ALLOC_CHOICE;
        }
        // Master must allocate either explicit or poll or both
        else if ((alloc_request & (EXPLICIT_CONXN | IO_POLL_CONXN)) == 0)
        {
            error = INVALID_ATTRIB_VALUE;
            additional_code = INVALID_ALLOC_CHOICE;
        }
        // check to see if Master is trying to allocate a connection already allocated
        else if (((alloc_request & EXPLICIT_CONXN) && (allocation.choice & EXPLICIT_CONXN)) ||
                  ((alloc_request & IO_POLL_CONXN) && (allocation.choice & IO_POLL_CONXN)))
        {
            error = ALREADY_IN_STATE;
            additional_code = INVALID_ALLOC_CHOICE;
        }
        else // passed validation tests
        {
            allocation.my_master = master_mac_id;
            global_status |= OWNED;

            // send message to explicit connection object
            if (alloc_request & EXPLICIT_CONXN)
            {
                allocation.choice |= EXPLICIT_CONXN;
                explicit_conxn->set_state(ESTABLISHED);
            }

            if (alloc_request & IO_POLL_CONXN)
            {
                allocation.choice |= IO_POLL_CONXN;
                io_poll_conxn->set_state(CONFIGURING);
            }

            // prepare response
            response[0] = request[0] & NON_FRAGMENTED;
            response[1] = service | SUCCESS_RESPONSE;
            response[2] = 0;   // 8/8 message format
            response[LENGTH] = 3;
        }
        break;



        case RELEASE_CONNECTIONS:
        release_request = request[4];
        if (release_request == 0)
        {
            error = INVALID_ATTRIB_VALUE;
            additional_code = INVALID_ALLOC_CHOICE;
        }
        // must not try to release something we don't support
        else if (release_request & 0xFC)
        {
            error = RESOURCE_UNAVAILABLE;
            additional_code = INVALID_ALLOC_CHOICE;
        }
        // the connection must exist to be released
        else if (((release_request & EXPLICIT_CONXN) &&
                  (explicit_conxn->get_state() == NON_EXISTENT)) ||
                  ((release_request & IO_POLL_CONXN) &&
                  (io_poll_conxn->get_state() == NON_EXISTENT)))
        {
            error = ALREADY_IN_STATE;
            additional_code = INVALID_ALLOC_CHOICE;
        }
        else // passed validation tests
        {
            if (release_request & EXPLICIT_CONXN)
            {
                allocation.choice &= (~EXPLICIT_CONXN);
                explicit_conxn->set_state(NON_EXISTENT);
            }
            if (release_request & IO_POLL_CONXN)
            {
                allocation.choice &= (~IO_POLL_CONXN);
                io_poll_conxn->set_state(NON_EXISTENT);
            }
            // If neither connection is in established state, release everything
            if ((explicit_conxn->get_state() != ESTABLISHED) &&
                 (io_poll_conxn->get_state() != ESTABLISHED))
            {
            explicit_conxn->set_state(NON_EXISTENT);
                io_poll_conxn->set_state(NON_EXISTENT);
                allocation.choice = 0;
            global_status &= (~OWNED);
                allocation.my_master = DEFAULT_MASTER_MAC_ID;
         }

            response[0] = request[0] & NON_FRAGMENTED;
            response[1] = service | SUCCESS_RESPONSE;
            response[LENGTH] = 2;
        }
        break;


        default:
        error = RESOURCE_UNAVAILABLE;
        additional_code = INVALID_UNC_REQUEST;
        break;
    }

    if (error)
    {
        response[0] = request[0] & NON_FRAGMENTED;
        response[1] = ERROR_RESPONSE;
        response[2] = error;
        response[3] = additional_code;
        response[LENGTH] = 4;
    }
    return OK;
}




// Handles Explicit request to the DeviceNet Object
void DEVICENET::handle_explicit(UCHAR request[], UCHAR response[])
{
    UINT service, attrib, error;

    service = request[1];
    attrib = request[4];
    error = 0;
    memset(response, 0, BUFSIZE);

    switch(service)
    {
        case GET_REQUEST:
        switch(attrib)
        {
            case 1: // get this device's mac id
            response[0] = request[0] & NON_FRAGMENTED;
            response[1] = service | SUCCESS_RESPONSE;
            response[2] = mac_id;
            response[LENGTH] = 3;
            break;

            case 2: // get baud rate
            response[0] = request[0] & NON_FRAGMENTED;
            response[1] = service | SUCCESS_RESPONSE;
            response[2] = baud_rate;
            response[LENGTH] = 3;
            break;

            case 3: // get bus off int value
            response[0] = request[0] & NON_FRAGMENTED;
            response[1] = service | SUCCESS_RESPONSE;
            response[2] = bus_off_int;
            response[LENGTH] = 3;
            break;

            case 5: // get allocation information struct
            response[0] = request[0] & NON_FRAGMENTED;
            response[1] = service | SUCCESS_RESPONSE;
            response[2] = allocation.choice;
            response[3] = allocation.my_master;
            response[LENGTH] = 4;
            break;

            default:
            error = ATTRIB_NOT_SUPPORTED;
            break;
        }
        break;


        case ALLOCATE_CONNECTIONS:
        // request to allocate connections received through the message router
        // forward the request to the unconnected port handler
        handle_unconnected_port(request, response);
        break;


        case RELEASE_CONNECTIONS:
        // request to release connections received through the message router
        // forward the request to the unconnected port handler
        handle_unconnected_port(request, response);
        break;



        default:
        error = SERVICE_NOT_SUPPORTED;
        break;
    }

    if (error)
    {
        response[0] = request[0] & NON_FRAGMENTED;
        response[1] = ERROR_RESPONSE;
        response[2] = error;
        response[3] = NO_ADDITIONAL_CODE;
        response[LENGTH] = 4;
    }
}


// Send a DUP MAC check response message.  This is a message sent in
// response to receiving a DUP MAC check message (with my MAC ID) from
// another device which is hoping to go on-line.  Sending this message
// keeps the offending device from going on-line.
void DEVICENET::send_dup_mac_response(void)
{
    // put bytes into CAN chip msg object #7 and send
   // load CAN message config register - msg length = 7
    pokeb(CAN_BASE, 0x76, 0x78);

    // load data area of CAN chip
    pokeb(CAN_BASE, 0x77, 0x80);			// indicate response message
    pokeb(CAN_BASE, 0x78, LOBYTE(vendor_id));
    pokeb(CAN_BASE, 0x79, HIBYTE(vendor_id));
    pokeb(CAN_BASE, 0x7A, (UCHAR)(serial));
    pokeb(CAN_BASE, 0x7B, (UCHAR)(serial >> 8));
    pokeb(CAN_BASE, 0x7C, (UCHAR)(serial >> 16));
    pokeb(CAN_BASE, 0x7D, (UCHAR)(serial >> 24));

    pokeb(CAN_BASE, 0x71, 0x66);      // set msg object #7 transmit request
}


// Consume an incoming DUP MAC check message from another device
int DEVICENET::consume_dup_mac(UCHAR request[])
{
    // If message is a response to our dup MAC ID check message
    // or if we are not on-line yet, error out.  Do not go on-line
    if (((request[0] & 0x80) != 0) ||      	// message is a response
        ((global_status & ON_LINE) == 0))      // we are off line
    {
        global_status |= DUP_MAC_FAULT;        // set dup MAC error
        return NO_RESPONSE;               		// send no response
    }
    return OK;                                // message consumed successfully
}


// Class Revisions
UINT CONNECTION::class_revision = 1;
UINT DISCRETE_INPUT_POINT::class_revision = 1;
UINT DISCRETE_OUTPUT_POINT::class_revision = 1;

void main(void)
{
    DISCRETE_INPUT_POINT *sensor = new DISCRETE_INPUT_POINT[NUM_SENSORS-1];
    for(UCHAR i=1; i<=NUM_SENSORS; i++)
        sensor[i-1].init_obj(i); // sensor instance 1 is sensor[0]
}
