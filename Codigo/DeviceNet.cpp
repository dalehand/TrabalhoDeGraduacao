#include <stdio.h>
#include <string.h>
#include <math.h>
#include "DeviceNet.h"
#include "SCan.h"

class CONNECTION
{
private:
    static UINT class_revision;
    static UCHAR path_in[PATH_SIZE];
    static UCHAR path_out[PATH_SIZE];
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


    UCHAR rcve_index, xmit_index;
    UCHAR my_rcve_fragment_count, my_xmit_fragment_count;
    UCHAR xmit_fragment_buf[BUFSIZE];
    UCHAR rcve_fragment_buf[BUFSIZE];
    UCHAR ack_timeout_counter;

public:
    UCHAR get_state(void);
    void set_state(UCHAR);
    UCHAR get_timeout_action(void);
    static void handle_class_inquiry(UCHAR*, UCHAR*);
    void handle_explicit(UCHAR*, UCHAR*);
    int link_consumer(UCHAR*);
    void link_producer(UCHAR*);
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
        produced_connection_size = 0xFFFF; // Message router maximum amount of data = 64 bit produced
        consumed_connection_size = 0xFFFF; // Message router maximum amount of data = 8 bit consumed
        expected_packet_rate = 2500; // Default value (CIPv3-3.30)
        watchdog_timeout_action = 0x01; // auto delete (CIPv3-3.30)
        produced_connection_path_length = 0;
        consumed_connection_path_length = 0;
        memset(produced_connection_path, 0, 10);
        memset(consumed_connection_path, 0, 10);
        production_inhibit_time = 0;
        rcve_index = 0;
        xmit_index = 0;
        my_rcve_fragment_count = 0;
        my_xmit_fragment_count = 0;
        memset(rcve_fragment_buf, 0, BUFSIZE);
        memset(xmit_fragment_buf, 0, BUFSIZE);
        ack_timeout_counter = 0;
        global_timer[EXPLICIT] = (expected_packet_rate / 50) * 4;

    }
    if((instance == IO_POLL)&&(state == CONFIGURING))
    {

        transport_class_trigger = 0x82; // 1 (Dir=Serv) 000 (ProdTrig=Cyclic) 0010 (TranspClass=2 do not prepend a 16-bit sequence CIPv3-3.4) CIPv1-3.27
        produced_connection_id = 0x3FF; // MessageGroup1 CIPv3-3.4 CIPv3-2.4
        consumed_connection_id = 0x5FD; // MessageGroup2 CIPv3-3.4 CIPv3-2.4
        initial_comm_characteristics = 0x01; // 0 = Produce across message group 1; 1 = Consume across message group 2(destination) CIPv3-6/7
        produced_connection_size = 8; // i/o poll response length in bytes (64 bits for 64 discrete inputs)
        consumed_connection_size = 1; // i/o poll request length in bytes (8 bits for 8 discrete outputs)
        expected_packet_rate = 0; // Default value (CIPv3-3.30)
        watchdog_timeout_action = 0x00; // go to timed-out (CIPv3-3.30)
        produced_connection_path_length = 6;
        consumed_connection_path_length = 6;
        memcpy(produced_connection_path, path_in, 6);
        memcpy(consumed_connection_path, path_out, 6);
        production_inhibit_time = 0;
        rcve_index = 0;
        xmit_index = 0;
        my_rcve_fragment_count = 0;
        my_xmit_fragment_count = 0;
        memset(rcve_fragment_buf, 0, BUFSIZE);
        memset(xmit_fragment_buf, 0, BUFSIZE);
        ack_timeout_counter = 0;

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
            if (instance_type == EXPLICIT_TYPE)
            {
                // Path is NULL for Explicit Connection
                response[LENGTH] = 2;
            }
            else  // Path for data sent in I/O Poll response
            {
                memcpy(&response[2], consumed_connection_path, 6);
                response[LENGTH] = 8;
            }
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
        /*******************************************/
        // In I/O message master must send 8 bits (1 byte) of discrete outputs
        if (request[LENGTH] == 1) return OK;
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
        if (rcve_index > consumed_connection_size)
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
            if (rcve_index > consumed_connection_size)
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
            if (rcve_index > consumed_connection_size)
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

void CONNECTION::link_producer(UCHAR response[])
{
    UCHAR length, bytes_left, i, fragment_count, ack_status;
    static UCHAR copy[BUFSIZE];

    length = response[LENGTH];

    if (instance == IO_POLL)
    {
        // load io poll response into can chip object #9
        for (i=0; i < length; i++)  						// load CAN data
        {
            //pokeb(CAN_BASE, (0x57 + i), response[i]);
            global_CAN_write[i] = response[i];
        }
        //pokeb(CAN_BASE, 0x56, ((length << 4) | 0x08));	// load config register
        global_CAN_write[LENGTH] = length;
        can_id_write = 0b01111111100;
        //pokeb(CAN_BASE, 0x51, 0x66);      					// set transmit request
        write_flag = 1;
    }


    else if (response[MESSAGE_TAG] == ACK_TIMEOUT)
    {
        ack_timeout_counter++;
        if (ack_timeout_counter == 1)
        {
            // Load last explicit fragment send again
            length = copy[LENGTH];
            for (i=0; i < length; i++)  						// load data into CAN
            {
                //pokeb(CAN_BASE, (0x67 + i), copy[i]);
                global_CAN_write[i] = copy[i];
            }
            //pokeb(CAN_BASE, 0x66, ((length << 4) | 0x08));	// load config resister
            global_CAN_write[LENGTH] = length;
            //pokeb(CAN_BASE, 0x61, 0x66);      					// set transmit request
            can_id_write= 0b10111100011;
            write_flag=1;
            global_timer[ACK_WAIT] = 20;

        }
        if (ack_timeout_counter == 2)
        {
            // abort trying to send fragmented message
            xmit_index = 0;
            my_xmit_fragment_count = 0;
            ack_timeout_counter = 0;
            global_timer[ACK_WAIT] = 0;
        }
    }


    else if (response[MESSAGE_TAG] == RECEIVED_ACK)
    {
        fragment_count = response[1] & 0x3F;
        ack_status = response[2];
        if (my_xmit_fragment_count == fragment_count)
        {
            // If Master returned a bad ACK status, reset everything
            // and abort the attempt to send message
            if (ack_status != 0)
            {
                xmit_index = 0;
                my_xmit_fragment_count = 0;
                ack_timeout_counter = 0;
                global_timer[ACK_WAIT] = 0;
            }

            // Master's ACK was OK, so send next fragment unless we were done sending
            // Keep a copy of what we are sending
            else
            {
                if (xmit_index >= xmit_fragment_buf[LENGTH])
                {
                    // got ACK to out final fragment so reset everything
                    xmit_index = 0;
                    my_xmit_fragment_count = 0;
                    ack_timeout_counter = 0;
                    global_timer[ACK_WAIT] = 0;
                }
                else
                {
                    // Send another fragment
                    // Figure out how many bytes are left to send
                    bytes_left = xmit_fragment_buf[LENGTH] - xmit_index;
                    my_xmit_fragment_count++;
                    ack_timeout_counter = 0;
                    global_timer[ACK_WAIT] = 20;				// restart timer for 1 second
                    // Load the first byte of the fragment
                    copy[0] = response[0] | 0x80;
                    //pokeb(CAN_BASE, 0x67, copy[0]);
                    if (bytes_left > 6)			// this is a middle fragment
                    {
                        copy[1] = MIDDLE_FRAG | my_xmit_fragment_count;
                        //pokeb(CAN_BASE, 0x68, copy[1]);
                        length = 8;
                    }
                    else   							// this is the last fragment
                    {
                        copy[1] = LAST_FRAG | my_xmit_fragment_count;
                        //pokeb(CAN_BASE, 0x68, copy[1]);
                        length = bytes_left + 2;
                    }
                    // Put in actual data
                    for (i = 2; i < length; i++)  // put up to 6 more bytes in CAN chip
                    {
                        copy[i] = xmit_fragment_buf[xmit_index];
                        //pokeb(CAN_BASE, (0x67 + i), copy[i]);
                        xmit_index++;
                    }
                    copy[LENGTH] = length;
                    //pokeb(CAN_BASE, 0x66, ((length << 4) | 0x08));	// load config resister
                    //pokeb(CAN_BASE, 0x61, 0x66);      // set msg object transmit request
                }
            }
        }
    }


    // Send this message in response to receiving an explicit fragment
    // from the Master that the link comsumer has validated as OK
    else if (response[MESSAGE_TAG] == SEND_ACK)
    {
        length = 3;												// This is a 3 byte message
        //pokeb(CAN_BASE, 0x67, (response[0] | 0x80));
        //pokeb(CAN_BASE, 0x68, (response[1] | ACK_FRAG));
        //pokeb(CAN_BASE, 0x69, 0);								// ack status = OK
        //pokeb(CAN_BASE, 0x66, ((length << 4) | 0x08));	// load config resister
        //pokeb(CAN_BASE, 0x61, 0x66);      					// set transmit request
    }



    // Send this message in response to receiving an explicit fragment
    // from the Master that exceeded the byte count limit for the connection
    else if (response[MESSAGE_TAG] == ACK_ERROR)
    {
        length = 3;												// This is a 3 byte message
        //pokeb(CAN_BASE, 0x67, (response[0] | 0x80));
        //pokeb(CAN_BASE, 0x68, (response[1] | ACK_FRAG));
        //pokeb(CAN_BASE, 0x69, 1);								// ack status = TOO MUCH DATA
        //pokeb(CAN_BASE, 0x66, ((length << 4) | 0x08));	// load config resister
        //pokeb(CAN_BASE, 0x61, 0x66);      					// set transmit request
    }



    else if (length <= 8)		// Send complete Explicit message
    {
        // load explicit response into can chip object #3
        for (i=0; i < length; i++)  						// load data into CAN
        {
            //pokeb(CAN_BASE, (0x67 + i), response[i]);
        }
        //pokeb(CAN_BASE, 0x66, ((length << 4) | 0x08));	// load config resister
        //pokeb(CAN_BASE, 0x61, 0x66);      					// set transmit request
    }



    else if (length > 8)       // Send first Explicit message fragment
    {
        // load explicit response into a buffer to keep around a while
        memcpy(xmit_fragment_buf, response, BUFSIZE);
        length = 8;
        xmit_index = 0;
        my_xmit_fragment_count = 0;
        ack_timeout_counter = 0;
        // Load first fragment into can chip object #3
        copy[0] = response[0] | 0x80;
        //pokeb(CAN_BASE, 0x67, copy[0]);
        xmit_index++;
        // Put in fragment info
        copy[1] = FIRST_FRAG | my_xmit_fragment_count;
        //pokeb(CAN_BASE, 0x68, copy[1]);
        // Put in actual data
        for (i = 2; i < 8; i++)  // put 6 more bytes in CAN chip
        {
            copy[i] = xmit_fragment_buf[xmit_index];
            //pokeb(CAN_BASE, (0x67 + i), copy[i]);
            xmit_index++;
        }
        copy[LENGTH] = length;
        //pokeb(CAN_BASE, 0x66, ((length << 4) | 0x08));	// load config resister
        //pokeb(CAN_BASE, 0x61, 0x66);      					// set msg object transmit request
        global_timer[ACK_WAIT] = 20;							// start timer to wait for ack
    }
}

class ASSEMBLY
{
    private:
    static UINT class_revision;
    UCHAR data[NUM_IN];

    DISCRETE_INPUT_POINT *inputsas;
    DISCRETE_OUTPUT_POINT *outputsas;

    public:
    ASSEMBLY(DISCRETE_INPUT_POINT *inputsmain, DISCRETE_OUTPUT_POINT *outputmain)
    {
        inputsas = inputsmain;
        outputsas = outputmain;
   }
    static void handle_class_inquiry(UCHAR*, UCHAR*);
    void handle_explicit(UCHAR*, UCHAR*);
    void update_data(void);
    void handle_io_poll_request(UCHAR*, UCHAR*);

};

// Handle an Explicit request directed to the class
void ASSEMBLY::handle_class_inquiry(UCHAR request[], UCHAR response[])
{
    UINT service, attrib, error;

    service = request[1];
    attrib = request[4];
    error = FALSE;
    memset(response, 0, BUFSIZE);

    switch(service)
    {
        case GET_REQUEST:
        switch(attrib)
        {
            case 1:  // handle revision attribute of class
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




// Handle an explicit request to the assembly object
void ASSEMBLY::handle_explicit(UCHAR request[], UCHAR response[])
{
    UINT service, attrib, error;
    UCHAR temp;
    service = request[1];
    attrib = request[4];
    error = FALSE;
    memset(response, 0, BUFSIZE);

    switch(service)
    {
        case GET_REQUEST:
        switch(attrib)
        {
            case 3:   // get data - array of 3 bytes
            response[0] = request[0] & NON_FRAGMENTED;
            response[1] = service | SUCCESS_RESPONSE;
            response[2] = data[0];
            response[3] = data[1];
            response[4] = data[2];
            response[LENGTH] = 5;
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

// Runs every 0.25 seconds to update the local copy
// This keeps the values up to date, and ready to send.
void ASSEMBLY::update_data(void)
{
    int count = 0;

    while(count<NUM_IN)
    {
        data[count] = inputsas[count]->get_value;
        count++;
    }
    // the assembly object keeps local copies of the values it needs to
    // send in the I/O POLL response.  It is faster that way.
    //data[0] = identity->get_state();
    //data[1] = temperature_sensor->get_value();
    //data[2] = humidity_sensor->get_value();
}



// Handle an I/O Poll request for data
void ASSEMBLY::handle_io_poll_request(UCHAR request[], UCHAR response[])
{
    BOOL val;
    // This is an input assembly so it does not expect to receive data
    // from master. It returns 3 bytes of data to send in the io poll response
    for (count = 0; count<NUM_OUT; count++)
    {
        val = (request[0] & (0b00000001 << (count-1)));
        ouputsas[count]->set_value(val);
    }

    memset(response, 0, BUFSIZE);
    memcpy(response, data, 3);
    response[LENGTH] = 3;
}


class DISCRETE_INPUT_POINT
{
private:
    BOOL value;
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
    int count =0;
    for(count=0;count<8;count++){
        digitalWrite(pinin[0], instance & (0x0001 << count));
    }
    value = digitalRead(pinin[8]);
    return value;
}



class DISCRETE_OUTPUT_POINT
{
private:
    BOOL value;
    UCHAR instance;
    BOOL status;
    static UINT class_revision;
public:
    static void handle_class_inquiry(UCHAR*, UCHAR*);
    void handle_explicit(UCHAR*, UCHAR*);
    void init_obj(UCHAR);
    void set_value(BOOL);
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

void DISCRETE_OUTPUT_POINT::set_value(BOOL val)
{
    value = val;
    digitalWrite( pinout[instance], val);
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
    //pokeb(CAN_BASE, 0x76, 0x78);
    // load data area of CAN chip

    can_id_write = 0b10111100111;
    global_CAN_write[0] = 0x80;
    global_CAN_write[1] = LOBYTE(vendor_id);
    global_CAN_write[2] = HIBYTE(vendor_id);
    global_CAN_write[3] = (UCHAR)(serial);
    global_CAN_write[4] = (UCHAR)(serial >> 8);
    global_CAN_write[5] = (UCHAR)(serial >> 16);
    global_CAN_write[6] = (UCHAR)(serial >> 24);
    global_CAN_write[LENGTH] = 7;
    write_flag = 1;

    //pokeb(CAN_BASE, 0x77, 0x80);			// indicate response message
    //pokeb(CAN_BASE, 0x78, LOBYTE(vendor_id));
    //pokeb(CAN_BASE, 0x79, HIBYTE(vendor_id));
    //pokeb(CAN_BASE, 0x7A, (UCHAR)(serial));
    //pokeb(CAN_BASE, 0x7B, (UCHAR)(serial >> 8));
    //pokeb(CAN_BASE, 0x7C, (UCHAR)(serial >> 16));
    //pokeb(CAN_BASE, 0x7D, (UCHAR)(serial >> 24));

    //pokeb(CAN_BASE, 0x71, 0x66);      // set msg object #7 transmit request
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


class IDENTITY
{
    private:
    static UINT class_revision;
    UINT vendor_id;
    UINT device_type;
    UINT product_code;
    struct
    {
        UCHAR major;
        UCHAR minor;
    } revision;

    ULONG serial;
    char product_name[40];
    ULONG device_clock, dup_mac_clock;
    CONNECTION *explicitcon, *io_poll;


    public:
    UINT status;
    UCHAR state;
    static void handle_class_inquiry(UCHAR*, UCHAR*);
    void handle_explicit(UCHAR*, UCHAR*);
    UCHAR get_state(void);
    void device_self_test(void);
    void send_dup_mac_check_message(void);
    void update_device(void);
    IDENTITY(UINT id, ULONG s, CONNECTION *ex, CONNECTION *io)  // constructor
    {
        device_clock = 0;
        dup_mac_clock = 0;
        vendor_id = id;
        serial = s;
        explicitcon = ex;
        io_poll = io;
        device_type = 0;  				// generic device
        product_code = 1;       		// product model within a device type
        revision.major = 1;     		// rev level of product model
        revision.minor = 0;
        status = 0;             		// status of the device
        state = 0;                    // state of the device
        memset(product_name, 0, 40);
        strcpy(product_name, "MODULO IO");
    }
};



// Handle Explicit requests to class
void IDENTITY::handle_class_inquiry(UCHAR request[], UCHAR response[])
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


// Handle Explicit requests to Identity Object
void IDENTITY::handle_explicit(UCHAR request[], UCHAR response[])
{
    UINT service, attrib, error;
    UCHAR reset_type;

    service = request[1];
    attrib = request[4];
    error = 0;
    memset(response, 0, BUFSIZE);

    switch(service)
    {
        case GET_REQUEST:
        switch(attrib)
        {
            case 1:  // get ODVA vendor ID
            response[0] = request[0] & NON_FRAGMENTED;
            response[1] = service | SUCCESS_RESPONSE;
            response[2] = LOBYTE(vendor_id);
            response[3] = HIBYTE(vendor_id);
            response[LENGTH] = 4;
            break;

            case 2:  // get device type
            response[0] = request[0] & NON_FRAGMENTED;
            response[1] = service | SUCCESS_RESPONSE;
            response[2] = LOBYTE(device_type);
            response[3] = HIBYTE(device_type);
            response[LENGTH] = 4;
            break;

            case 3:  // get product code
            response[0] = request[0] & NON_FRAGMENTED;
            response[1] = service | SUCCESS_RESPONSE;
            response[2] = LOBYTE(product_code);
            response[3] = HIBYTE(product_code);
            response[LENGTH] = 4;
            break;

            case 4:  // get revision level
            response[0] = request[0] & NON_FRAGMENTED;
            response[1] = service | SUCCESS_RESPONSE;
            response[2] = revision.major;
            response[3] = revision.minor;
            response[LENGTH] = 4;
            break;

            case 5:  // get summary status
            response[0] = request[0] & NON_FRAGMENTED;
            response[1] = service | SUCCESS_RESPONSE;
            response[2] = LOBYTE(status);
            response[3] = HIBYTE(status);
            response[LENGTH] = 4;
            break;

            case 6:  // get serial number - 4 byte
            response[0] = request[0] & NON_FRAGMENTED;
            response[1] = service | SUCCESS_RESPONSE;
            response[2] = (UCHAR)(serial);
            response[3] = (UCHAR)(serial >> 8);
            response[4] = (UCHAR)(serial >> 16);
            response[5] = (UCHAR)(serial >> 24);
            response[LENGTH] = 6;
            break;

            case 7:  // get product name (short string format)
            response[0] = request[0] & NON_FRAGMENTED;
            response[1] = service | SUCCESS_RESPONSE;
            response[2] = (UCHAR)strlen(product_name);
            strcpy_UCHAR(&response[3], product_name);
            response[LENGTH] = response[2] + 3;
            break;

            case 8:  // get device state
            response[0] = request[0] & NON_FRAGMENTED;
            response[1] = service | SUCCESS_RESPONSE;
            response[2] = state;
            response[LENGTH] = 3;
            break;


            default:
            error = ATTRIB_NOT_SUPPORTED;
            break;
        }
        break;


        case RESET_REQUEST:
      // If reset type is not specified, default to type zero
        if (request[LENGTH] == 4) reset_type = 0;
        else reset_type = request[4];

        if (reset_type == 0)    		// simulate off/on cycle
        {
            // return success response now, can't do it after reset
            response[0] = request[0] & NON_FRAGMENTED;
            response[1] = service | SUCCESS_RESPONSE;
            response[LENGTH] = 2;
            global_event |= FULL_RESET;      // set reset bit
        }
        else if (reset_type == 1)
        {
            // reset to out-of-box condition, then simulate power off/on cycle
            // return success response before if can't do it after
            response[0] = request[0] & NON_FRAGMENTED;
            response[1] = service | SUCCESS_RESPONSE;
            response[LENGTH] = 2;
            global_event |= FULL_RESET;      // set reset bit
        }
        else  error = INVALID_PARAMETER;
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

// Returns the state of the device
UCHAR IDENTITY::get_state(void)
{
    return state;
}

// Handles device startup & LED states
void IDENTITY::device_self_test(void)
{
   //In case there is an test in the module
}

// Sends a DUP MAC ID check request.
// Two of these are sent during the startup sequence.
void IDENTITY::send_dup_mac_check_message(void)
{
    // Send first dup MAC ID check message
    // Put message into CAN chip msg object #7 and send
    // Load CAN message config register - msg length = 7
    //pokeb(CAN_BASE, 0x76, 0x78);

    can_id_write = 0b10111100111;
    global_CAN_write[0] = 0x00;
    global_CAN_write[1] = LOBYTE(vendor_id);
    global_CAN_write[2] = HIBYTE(vendor_id);
    global_CAN_write[3] = (UCHAR)(serial);
    global_CAN_write[4] = (UCHAR)(serial >> 8);
    global_CAN_write[5] = (UCHAR)(serial >> 16);
    global_CAN_write[6] = (UCHAR)(serial >> 24);
    global_CAN_write[LENGTH] = 7;
    write_flag = 1;

    // Load data area of CAN chip
    //pokeb(CAN_BASE, 0x77, 0);		  // request flag (not response)
    //pokeb(CAN_BASE, 0x78, LOBYTE(vendor_id));
    //pokeb(CAN_BASE, 0x79, HIBYTE(vendor_id));
    //pokeb(CAN_BASE, 0x7A, (UCHAR)(serial));
    //pokeb(CAN_BASE, 0x7B, (UCHAR)(serial >> 8));
    //pokeb(CAN_BASE, 0x7C, (UCHAR)(serial >> 16));
    //pokeb(CAN_BASE, 0x7D, (UCHAR)(serial >> 24));
    //pokeb(CAN_BASE, 0x71, 0x66);      // set msg object #7 transmit request

}



// Handles operation of overall device, checks for errors, handles LEDs,
// This runs every 0.25 second.  If the startup self-test is OK,
// the Device sends two dup MAC ID check messages
void IDENTITY::update_device(void)
{
    UCHAR explicit_conxn_state, io_poll_conxn_state;
    UCHAR temp;


    // For first 2 seconds (8 clock ticks) do self-test
    if (device_clock < 8) device_self_test();
    else if ((dup_mac_clock <= 8) &&
              ((global_status & DEVICE_FAULT) == 0) &&
              ((global_status & NETWORK_FAULT) == 0))
    {
        if (global_status & LONELY_NODE) dup_mac_clock = 0;
        else
        {
            // This generates two dup mac check messages one second apart
            // If we are the only node on the network, keep sending
            switch(dup_mac_clock)
            {
                case 0:      // start
                send_dup_mac_check_message();
                break;

                case 4:      // 1 second has elapsed
                send_dup_mac_check_message();
                break;

                case 8:      // 2 seconds have elapsed
                global_status |= ON_LINE;
                break;
            }
            dup_mac_clock++;
        }
    }

/*    if (device_clock >= 8) // self-test done, update status and module LED
    {
        // Copy the global status into the identity object status attribute
        status = global_status & 0x0F05;  				// zero out some bits
        if (global_status & DEVICE_FAULT)
        {
            // module LED red
            state = 5;		// indicate device fault
            //temp = peekb(PIO_BASE, PORTC);
            temp &= 0xD0;		// red on
            temp |= 0x10;		// grn off
            //pokeb(PIO_BASE, PORTC, temp);
        }
        else if (global_status & OPERATIONAL)
        {
            // module LED green
            state = 3;			// indicate device is operational
            //temp = peekb(PIO_BASE, PORTC);
            temp &= 0xE0;		// grn on
            temp |= 0x20;		// red off
            //pokeb(PIO_BASE, PORTC, temp);
        }

        // Next, handle network bicolor LED
        // Check state of connections
        explicit_conxn_state = explicitcon->get_state();
        io_poll_conxn_state = io_poll->get_state();

        if (global_status & NETWORK_FAULT)
        {
            // network LED steady red
            //temp = peekb(PIO_BASE, PORTC);
            temp &= 0x70;		// red on
            temp |= 0x40;		// grn off

            //pokeb(PIO_BASE, PORTC, temp);
        }
        else if ((global_status & ON_LINE) == 0)
        {
            // network LED off
            //temp = peekb(PIO_BASE, PORTC);
            temp |= 0xC0;		// both off
            //pokeb(PIO_BASE, PORTC, temp);
        }
        else if (io_poll_conxn_state == TIMED_OUT)
        {
            // network LED flashing red
            //temp = peekb(PIO_BASE, PORTC);
            if (((device_clock / 2) % 2) == 0)
            {
                temp &= 0x70;		// red on
                temp |= 0x40;		// grn off
                //pokeb(PIO_BASE, PORTC, temp);
            }
            else
            {
                temp |= 0xC0;		// both off
                //pokeb(PIO_BASE, PORTC, temp);
            }
        }
        else if ((io_poll_conxn_state != ESTABLISHED) &&
                    (explicit_conxn_state != ESTABLISHED))
        {
            // network LED flashing green
            //temp = peekb(PIO_BASE, PORTC);
            if (((device_clock / 2) % 2) == 0)
            {
                temp &= 0xB0;		// grn on
                temp |= 0x80;		// red off
                //pokeb(PIO_BASE, PORTC, temp);
            }
            else
            {
                temp |= 0xC0;		// both on
                //pokeb(PIO_BASE, PORTC, temp);
            }
        }
        else if ((io_poll_conxn_state == ESTABLISHED) ||
                    (explicit_conxn_state == ESTABLISHED))
        {
            // at least one connection is established
            // network LED steady green
            //temp = peekb(PIO_BASE, PORTC);
            temp &= 0xB0;		// grn on
            temp |= 0x80;		// red off
            //pokeb(PIO_BASE, PORTC, temp);
        }
    }*/
    device_clock++;
}

class ROUTER
{
    private:
    static UINT class_revision;
    // address of objects the router will need to send messages to

    DISCRETE_INPUT_POINT *discretein;
    DISCRETE_OUTPUT_POINT *discreteout;
    ASSEMBLY *assembly;
    IDENTITY *identity;
    DEVICENET *devicenet;
    CONNECTION *explicito, *io_poll;

    public:
    static void handle_class_inquiry(UCHAR*, UCHAR*);
    void handle_explicit(UCHAR*, UCHAR*);
    void route(UCHAR*, UCHAR*);
    ROUTER(DISCRETE_INPUT_POINT *in, DISCRETE_OUTPUT_POINT *out,IDENTITY *id,
             DEVICENET *dn, CONNECTION *ex, CONNECTION *io, ASSEMBLY *as)
    {
      // Initialize address of other objects
        discretein = in;
        discreteout = out;
        identity = id;
        devicenet = dn;
        explicito = ex;
        io_poll = io;
        assembly = as;
    }
};




// handle Explicit request to class
void ROUTER::handle_class_inquiry(UCHAR request[], UCHAR response[])
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



// Handles Explicit request to the Router Object
void ROUTER::handle_explicit(UCHAR request[], UCHAR response[])
{
    UINT service, attrib, error;

    service = request[1];
    attrib = request[4];
    error = 0;
    memset(response, 0, BUFSIZE);

    error = SERVICE_NOT_SUPPORTED;  // no services supported

    if (error)
    {
        response[0] = request[0] & NON_FRAGMENTED;
        response[1] = ERROR_RESPONSE;
        response[2] = error;
        response[3] = NO_ADDITIONAL_CODE;
        response[LENGTH] = 4;
    }
}




// Pass Explicit requests to appropriate object and get response
void ROUTER::route(UCHAR request[], UCHAR response[])
{
    UINT class_id, instance, error;
    int i = 1;
    class_id = request[2];
    instance = request[3];
    error = 0;
    memset(response, 0, BUFSIZE);

    switch(class_id)
    {
        case DISCRETE_INPUT_POINT_CLASS:
        if(instance == 0)
            DISCRETE_INPUT_POINT::handle_class_inquiry(request, response);
        else{
            while((i != -1)&&( i <= NUM_IN))
            {
                if(intance == i)
                {
                    discretein[i-1]->handle_class_inquiry(request, response);
                    i=-2;
                }
                i++;
            }
        }
        if(instance>NUM_IN)
            error = OBJECT_DOES_NOT_EXIST;
        break;

        case DISCRETE_OUTPUT_POINT_CLASS:
        if(instance == 0)
            DISCRETE_OUTPUT_POINT::handle_class_inquiry(request, response);
        else{
            while((i != -1)&&( i <= NUM_OUT))
            {
                if(intance == i)
                {
                    discreteout[i-1]->handle_class_inquiry(request, response);
                    i=-2;
                }
                i++;
            }
        }
        if(instance>NUM_IN)
            error = OBJECT_DOES_NOT_EXIST;
        break;

        case ASSEMBLY_CLASS:
        switch(instance)
        {
            case 0:  // direct this to the class
            ASSEMBLY::handle_class_inquiry(request, response);
            break;

            case 1:  // direct this to instance 1
            assembly->handle_explicit(request, response);
            break;

            default:
            error = OBJECT_DOES_NOT_EXIST;
            break;
        }
        break;



        case IDENTITY_CLASS:
        switch(instance)
        {
            case 0:     // direct this to the class
            IDENTITY::handle_class_inquiry(request, response);
            break;

            case 1:     // direct this to instance 1
            identity->handle_explicit(request, response);
            break;

            default:
            error = OBJECT_DOES_NOT_EXIST;
            break;
        }
        break;


        case DEVICENET_CLASS:
        switch(instance)
        {
            case 0:    // direct this to the class
            DEVICENET::handle_class_inquiry(request, response);
            break;

            case 1:    // direct this to instance 1
            devicenet->handle_explicit(request, response);
            break;

            default:
            error = OBJECT_DOES_NOT_EXIST;
            break;
        }
        break;


        case CONNECTION_CLASS:
        switch(instance)
        {
            case 0:    		// direct this to the class
            CONNECTION::handle_class_inquiry(request, response);
            break;

            case 1:        // direct this to instance 1
            explicito->handle_explicit(request, response);
         break;

            case 2:        // direct this to instance 2
            io_poll->handle_explicit(request, response);
            break;

            default:
            error = OBJECT_DOES_NOT_EXIST;
            break;
        }
        break;


        case ROUTER_CLASS:
        switch(instance)
        {
            case 0:		// direct this to the class
            handle_class_inquiry(request, response);
            break;

            case 1:     // direct this to instance 1
            handle_explicit(request, response);
            break;

            default:
            error = OBJECT_DOES_NOT_EXIST;
            break;

        }
        break;


        default:  // no such class
        error = OBJECT_DOES_NOT_EXIST;
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

// Class Revisions
UINT CONNECTION::class_revision = 1;
UINT DISCRETE_INPUT_POINT::class_revision = 1;
UINT DISCRETE_OUTPUT_POINT::class_revision = 1;
UINT IDENTITY::class_revision = 1;
UINT DEVICENET::class_revision = 2;
UINT ROUTER::class_revision = 1;

UCHAR CONNECTION::path_in[10] =  {0x20, 0x04, 0x24, 0x01, 0x30, 0x03}; // 0010 0000 0000 0100 / 0010 0100 0000 0001 / 0011 0000 0000 0011 (Apendix C - CIPv1)
                                                                    // 001 - Logical Segment
                                                                    // 000 - Class ID
                                                                    // 00 - 8-bit logical address
                                                                    // 00000100 - Class ID #4  => Assembly Object (CIPv1-5.43)
                                                                    // 001 - Logical segment
                                                                    // 001 - Instance ID
                                                                    // 00 - 8-bit logical address
                                                                    // 00000001 - Instance ID #1  => 1st instance of assembly class
                                                                    // 001 - Logical Segment
                                                                    // 100 - Attribute ID
                                                                    // 00 - 8-bit logical address
                                                                    // 00000011 - Attribute #3 => Data (CIPv1-5-44)

UCHAR CONNECTION::path_out[10] =  {0x20, 0x04, 0x28, 0x01, 0x30, 0x03}; // 0010 0000 0000 0100 / 0010 0100 0000 0001 / 0011 0000 0000 0011 (Apendix C - CIPv1)
                                                                    // 001 - Logical Segment
                                                                    // 000 - Class ID
                                                                    // 00 - 8-bit logical address
                                                                    // 00000100 - Class ID #4  => Assembly Object (CIPv1-5.43)
                                                                    // 001 - Logical segment
                                                                    // 010 - Instance ID
                                                                    // 00 - 8-bit logical address
                                                                    // 00000001 - Instance ID #1  => 1st instance of assembly class
                                                                    // 001 - Logical Segment
                                                                    // 100 - Attribute ID
                                                                    // 00 - 8-bit logical address
                                                                    // 00000011 - Attribute #3 => Data (CIPv1-5-44)

int main()
{

    UINT e; // Temporary variable that stores global_event
    UCHAR request[BUFSIZE];
    UCHAR response[BUFSIZE];
    DISCRETE_INPUT_POINT *input_point = new DISCRETE_INPUT_POINT[NUM_IN-1];
    DISCRETE_OUTPUT_POINT *output_point = new DISCRETE_OUTPUT_POINT[NUM_OUT-1];
    CONNECTION expl(EXPLICIT);
    CONNECTION io_poll(IO_POLL);
    DEVICENET devicenet(MAC_ID, BAUD_RATE, VENDOR_ID, SERIAL_ID, &expl, &io_poll);
    IDENTITY identity(VENDOR_ID, SERIAL_NUM, &expl, &io_poll);
    ROUTER router(&input_point, &output_point, &identity,
                      &devicenet, &expl, &io_poll, &assembly);

    for(UCHAR i=1; i<=NUM_IN; i++)
        input_point[i-1].init_obj(i); // sensor instance 1 is in[0]

    for(UCHAR i=1; i<=NUM_OUT; i++)
        output_point[i-1].init_obj(i); // sensor instance 1 is out[0]

    std::thread t1(&sock_can_read);
    std::thread t2(&sock_can_write);
    std::thread t3(&timer_func);

    while(1)
    {

        e = global_event;
        if ((e & IO_POLL_REQUEST) && !(e & (IO_POLL_REQUEST - 1)))
        {
            global_event &= ~IO_POLL_REQUEST; // clear bit
            // must be online with no critical errors
            if ((global_status & ON_LINE) && !(global_status & NETWORK_FAULT))
            {
                // Get request message from global ISR buffer
                memcpy(request, global_CAN_buf, BUFSIZE);
            if (io_poll.link_consumer(request))  	// try to consume the request
                {
                    assembly.handle_io_poll_request(request, response);
                    io_poll.link_producer(response); 	// produce response
                }
            }
        }


        // Incoming Explicit request message
        e = global_event;
        if ((e & EXPLICIT_REQUEST) && !(e & (EXPLICIT_REQUEST - 1)))
        {
            global_event &= ~EXPLICIT_REQUEST; // clear bit
            // must be online with no critical errors
            if ((global_status & ON_LINE) && !(global_status & NETWORK_FAULT))
            {
                // make sure message is from my master
                if ((global_CAN_buf[0] & 0x3F) == devicenet.allocation.my_master)
                {
                    // Get request message from global ISR buffer
                    memcpy(request, global_CAN_buf, BUFSIZE);
                    if (expl.link_consumer(request)) 	// try to consume the request
                    {
                        router.route(request, response);  	// route request & get response
                        expl.link_producer(response);   // produce response
                    }
                }
            }
        }


        e = global_event;
        if ((e & DUP_MAC_REQUEST) && !(e & (DUP_MAC_REQUEST - 1))) // Incoming message from another device
        {
            global_event &= ~DUP_MAC_REQUEST; // Clear bit on global event
            memcpy(request, global_CAN_buf, BUFSIZE);
            if(devicenet.consume_dup_mac(request)) // If receives a dup mac request with the same macid, respond to give error
                devicenet.send_dup_mac_response();
        }

        // Incoming message to the Unconnected Port
        e = global_event;
        if ((e & UNC_PORT_REQUEST) && !(e & (UNC_PORT_REQUEST - 1)))
        {
            global_event &= ~UNC_PORT_REQUEST;				// clear bit
            // must be online with no critical errors
            if ((global_status & ON_LINE) && !(global_status & NETWORK_FAULT))
            {
                // Get request message from global ISR buffer
                disable();
                memcpy(request, global_CAN_buf, BUFSIZE);
                enable();
                if (devicenet.handle_unconnected_port(request, response));
            {
                    // Note: I am not using link producer to send response because
                    // the Unconnected port should have its own direct connection to
                    // the network - So load response directly into can chip object #3
                //length = response[LENGTH];
                global_CAN_write[LENGTH] = response[LENGTH];
                    for (i=0; i < length; i++)  						// load data into CAN
                    {
                        //pokeb(CAN_BASE, (0x67 + i), response[i]);
                        global_CAN_write[i] = response[i];
                    }
                    //pokeb(CAN_BASE, 0x66, ((length << 4) | 0x08));	// load config resister
                    //pokeb(CAN_BASE, 0x61, 0x66);      					// set transmit request
                    write_flag = 1;
            }
          }
        }

        // Timeout of the Explicit connection
        e = global_event;
        if ((e & EXPLICIT_TIMEOUT) && !(e & (EXPLICIT_TIMEOUT - 1)))
        {
            global_event &= ~EXPLICIT_TIMEOUT;	// clear bit
            devicenet.handle_timeout(EXPLICIT_CONXN);
        }


        // Timeout of the I/O Poll connection
        e = global_event;
        if ((e & IO_POLL_TIMEOUT) && !(e & (IO_POLL_TIMEOUT - 1)))
        {
            global_event &= ~IO_POLL_TIMEOUT;	// clear bit
            devicenet.handle_timeout(IO_POLL_CONXN);
        }

        // Timeout waiting for an acknowlege message(to a fragment I sent)
          e = global_event;
          if ((e & ACK_WAIT_TIMEOUT) && !(e & (ACK_WAIT_TIMEOUT - 1)))
          {
              global_event &= ~ACK_WAIT_TIMEOUT;
              // Notify link producer that we have timed-out while waiting for an ACK
              request[MESSAGE_TAG] = ACK_TIMEOUT;
              explicit.link_producer(request);
          }

        e = global_event;
        if ((e & FULL_RESET) && !(e & (FULL_RESET - 1)))
        {
            global_event &= ~FULL_RESET;
            // Go into loop and let watchdog reset the device
            while (1);
        }
    }
    return 0;
}
