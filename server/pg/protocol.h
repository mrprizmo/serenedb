/*-------------------------------------------------------------------------
 *
 * protocol.h
 *		Definitions of the request/response codes for the wire protocol.
 *
 *
 * Portions Copyright (c) 1996-2023, PostgreSQL Global Development Group
 * Portions Copyright (c) 1994, Regents of the University of California
 *
 * src/include/libpq/protocol.h
 *
 *-------------------------------------------------------------------------
 */
#ifndef PROTOCOL_H
#define PROTOCOL_H

/* These are the request codes sent by the frontend. */

#define PQ_MSG_BIND 'B'
#define PQ_MSG_CLOSE 'C'
#define PQ_MSG_DESCRIBE 'D'
#define PQ_MSG_EXECUTE 'E'
#define PQ_MSG_FUNCTION_CALL 'F'
#define PQ_MSG_FLUSH 'H'
#define PQ_MSG_PARSE 'P'
#define PQ_MSG_QUERY 'Q'
#define PQ_MSG_SYNC 'S'
#define PQ_MSG_TERMINATE 'X'
#define PQ_MSG_COPY_FAIL 'f'
#define PQ_MSG_GSS_RESPONSE 'p'
#define PQ_MSG_PASSWORD_MESSAGE 'p'
#define PQ_MSG_SASL_INITIAL_RESPONSE 'p'
#define PQ_MSG_SASL_RESPONSE 'p'

/* These are the response codes sent by the backend. */

#define PQ_MSG_PARSE_COMPLETE '1'
#define PQ_MSG_BIND_COMPLETE '2'
#define PQ_MSG_CLOSE_COMPLETE '3'
#define PQ_MSG_NOTIFICATION_RESPONSE 'A'
#define PQ_MSG_COMMAND_COMPLETE 'C'
#define PQ_MSG_DATA_ROW 'D'
#define PQ_MSG_ERROR_RESPONSE 'E'
#define PQ_MSG_COPY_IN_RESPONSE 'G'
#define PQ_MSG_COPY_OUT_RESPONSE 'H'
#define PQ_MSG_EMPTY_QUERY_RESPONSE 'I'
#define PQ_MSG_BACKEND_KEY_DATA 'K'
#define PQ_MSG_NOTICE_RESPONSE 'N'
#define PQ_MSG_AUTHENTICATION_REQUEST 'R'
#define PQ_MSG_PARAMETER_STATUS 'S'
#define PQ_MSG_ROW_DESCRIPTION 'T'
#define PQ_MSG_FUNCTION_CALL_RESPONSE 'V'
#define PQ_MSG_COPY_BOTH_RESPONSE 'W'
#define PQ_MSG_READY_FOR_QUERY 'Z'
#define PQ_MSG_NO_DATA 'n'
#define PQ_MSG_PORTAL_SUSPENDED 's'
#define PQ_MSG_PARAMETER_DESCRIPTION 't'
#define PQ_MSG_NEGOTIATE_PROTOCOL_VERSION 'v'

/* These are the codes sent by both the frontend and backend. */

#define PQ_MSG_COPY_DONE 'c'
#define PQ_MSG_COPY_DATA 'd'

#endif /* PROTOCOL_H */
