package com.example.emvauth;

import org.springframework.http.HttpStatus;
import org.springframework.web.bind.annotation.ResponseStatus;

/** The request's ICC data (DE 55) is missing, malformed or inconsistent with the rest of the request. */
@ResponseStatus(HttpStatus.BAD_REQUEST)
class InvalidIccDataException extends RuntimeException {
    InvalidIccDataException(String message) {
        super(message);
    }
}
